/**
 * Infinitag Target – Live-Firmware (Infinitag Now, ESP-NOW v0x03)
 *
 * Ersetzt die alte WLAN/HTTP-Firmware (WiFiManager + GET auf die Wand):
 * kein Router, kein Captive-Portal mehr. Konfiguration kommt per Funk von
 * der Config-Box (CFG_WRITE); der Treffer geht als HIT_REPORT-Broadcast
 * raus und wird ueber die Schuetzen-ID dynamisch zur schiessenden Station
 * geroutet (PROTOCOL.md im Core-Repo).
 *
 * Spielfluss:
 *   Station schiesst IR-Telegramm (38 kHz, shooter_id + damage + CRC-4,
 *   IrTelegram.h; revidiert Punkt 41 am 2026-07-24) → TSOP4138 → ISR
 *   misst Mark/Space-Phasen und fuettert den IrtDecoder → gueltiger
 *   Frame = Treffer → HIT_REPORT sofort raus (Latenzbudget < 50 ms)
 *   → LED-Ring rote Welle + Schaltausgaenge feuern das Prop-Pattern.
 *
 * Zustandsmaschine:
 *   ARMED (Rainbow) → HIT (hit_time_ms: rote Welle + Switch-Pattern)
 *   → COOLDOWN (cooldown_ms: gedimmtes Blau-Pulsieren) → ARMED.
 *
 * Alles laeuft non-blocking in loop() – der zweite FreeRTOS-Task der
 * alten Firmware entfaellt (nichts blockiert mehr ausser dem SoftAP-
 * Update-Modus, der sowieso im Reboot endet).
 *
 * GPIO-Plan (Target-V3.2-PCB, unveraendert):
 *   GPIO15 → TSOP4138 OUT (active-LOW; Pin-Annahme aus dem alten Code,
 *            beim Bring-up verifizieren – Doc 04!)
 *   GPIO38 → 12x SK6812 RGBW LED-Ring (NEO_GRBW)
 *   GPIO21 → SW1   (potentialfrei, Optokoppler; sw_channels bit0)
 *   GPIO46 → SW-5V (NPN;                        sw_channels bit1)
 *   GPIO48 → SW-3.3V (NPN;                      sw_channels bit2)
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "FwMarker.h"
#include "IrTelegram.h"
#include "NowTarget.h"
#include "TargetSettings.h"
#include "WebUpdateService.h"

// Firmware identity for the config box's image store (Doc 21 E1).
INOW_FW_MARKER(inow::DEV_TARGET, TARGET_FW_MAJOR, TARGET_FW_MINOR,
               TARGET_FW_PATCH)

// ── Pins ─────────────────────────────────────────────────────────────────────
#define IR_PIN        15   // TSOP4138 OUT, active-LOW
#define LED_PIN       38   // SK6812 RGBW ring, data
#define LED_COUNT     12
#define LED_BRIGHT    255
#define SW1_PIN       21   // potential-free (optocoupler), sw_channels bit0
#define SW_5V_PIN     46   // NPN switch 5 V,               sw_channels bit1
#define SW_33V_PIN    48   // NPN switch 3.3 V,             sw_channels bit2

// ── LED ring / switches ──────────────────────────────────────────────────────
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

// ── Persistent config + ESP-NOW device logic ────────────────────────────────
static TargetSettings gSettings;
static NowTarget gNow;

// ── Target state machine ─────────────────────────────────────────────────────
enum TargetState : uint8_t { ARMED, HIT, COOLDOWN };
static TargetState gState = ARMED;
static uint32_t gHitMs = 0;        // time of the current hit
static uint32_t gStateUntil = 0;   // end of the HIT/COOLDOWN phase

// ── LED animation state ──────────────────────────────────────────────────────
static uint32_t gAnimPrevMs = 0;
static uint32_t gAnimInterval = 10;
static long gAnimStep = 0;

// ── Switch patterns (ported from the old firmware) ──────────────────────────
// Pattern 0: on for the whole hit time. Pattern 1: three pulses.
// Pattern 0's first step time is overwritten with hit_time_ms on each hit.
static bool swPattern[2][10] = {{true, false},
                                {true, false, true, false, true, false}};
static uint16_t swPatternTimes[2][10] = {{10000, 0},
                                         {700, 200, 200, 200, 200, 0}};
static const uint8_t swPatternSteps[2] = {2, 6};
static uint8_t gSwStep = 0;
static uint32_t gSwNextMs = 0;

// ── IR telegram reception (ISR, CHANGE on the TSOP output) ──────────────────
// Every edge closes a mark (TSOP LOW = carrier seen) or space phase; the
// measured duration feeds the IrtDecoder state machine from the core.
// Only frames with valid CRC come out – remote controls and the old
// v0x02 burst never produce a hit (PROTOCOL.md v0x03).
static inow::IrtDecoder gIrDecoder;
static volatile uint32_t gIrLastEdgeUs = 0;
static volatile uint16_t gIrFrame = 0;   // completed valid frame
static volatile bool gIrFramePending = false;

static void IRAM_ATTR irIsr() {
  const uint32_t now = micros();
  const uint32_t phaseUs = now - gIrLastEdgeUs;
  gIrLastEdgeUs = now;
  // Pin LOW now = a space just ended; pin HIGH now = a mark just ended.
  const bool markEnded = digitalRead(IR_PIN) == HIGH;
  if (gIrDecoder.feed(markEnded, phaseUs)) {
    uint16_t f;
    if (gIrDecoder.take(f)) {
      gIrFrame = f;
      gIrFramePending = true;
    }
  }
}

// ── Switches ─────────────────────────────────────────────────────────────────
// Only channels enabled in sw_channels are driven; disabled ones stay LOW.
static void setSw(bool on) {
  const uint8_t ch = gSettings.swChannels;
  digitalWrite(SW1_PIN, (on && (ch & 0x01)) ? HIGH : LOW);
  digitalWrite(SW_5V_PIN, (on && (ch & 0x02)) ? HIGH : LOW);
  digitalWrite(SW_33V_PIN, (on && (ch & 0x04)) ? HIGH : LOW);
}

static void switchPatternStart() {
  const uint8_t p = gSettings.swAnimation;
  swPatternTimes[0][0] = gSettings.hitTimeMs;  // pattern 0 = full hit time
  gSwStep = 0;
  gSwNextMs = millis() + swPatternTimes[p][0];
  setSw(swPattern[p][0]);
}

static void switchPatternTick() {
  const uint8_t p = gSettings.swAnimation;
  if (gSwStep >= swPatternSteps[p]) return;  // pattern finished
  if (millis() < gSwNextMs) return;
  gSwStep++;
  if (gSwStep >= swPatternSteps[p]) {
    setSw(false);
    return;
  }
  setSw(swPattern[p][gSwStep]);
  gSwNextMs = millis() + swPatternTimes[p][gSwStep];
}

// ── LED helpers ──────────────────────────────────────────────────────────────
static void setColor(uint32_t color) {
  for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, color);
  strip.show();
}

static void resetAnimation(uint32_t interval) {
  gAnimPrevMs = millis();
  gAnimStep = 0;
  gAnimInterval = interval;
}

// Idle: rainbow walk over the ring (ported from the old firmware).
static void animationRainbow() {
  strip.rainbow(gAnimStep);
  strip.show();
  gAnimStep = (gAnimStep < 65536) ? (gAnimStep + 256) : 0;
}

// Hit: red wave running around the ring, peak brightness fading out
// over the hit window (ported from the old firmware).
static void animationHit() {
  strip.clear();
  const long maxBrightness =
      map(millis(), gHitMs, gHitMs + gSettings.hitTimeMs, 0, 255);
  for (int i = 0; i < LED_COUNT; i++) {
    const int distance =
        (i <= gAnimStep) ? (gAnimStep - i) : (LED_COUNT - i + gAnimStep);
    strip.setPixelColor(
        i, strip.Color(map(distance, 0, LED_COUNT - 1, maxBrightness, 0), 0,
                       0, 0));
  }
  strip.show();
  gAnimStep = (gAnimStep < LED_COUNT - 1) ? (gAnimStep + 1) : 0;
}

// Cooldown: dim blue breathing – visibly "not armed yet".
static void animationCooldown() {
  const uint8_t b = 20 + (uint8_t)(15 * (1 + sinf(millis() / 300.0f)));
  setColor(strip.Color(0, 0, b, 0));
}

// Identify (config box): fast white pulse, overrides everything.
// Writes to the LED bus only on state changes (called every loop pass).
static void animationIdentify() {
  static bool init = false;
  static bool last = false;
  const bool on = (millis() / 200) % 2 == 0;
  if (!init || on != last) {
    init = true;
    last = on;
    setColor(on ? strip.Color(0, 0, 0, 180) : 0);
  }
}

// Boot/self test: R, G, B, W(die) one after the other.
static void ledTestPattern() {
  const uint32_t cols[4] = {
      strip.Color(255, 0, 0), strip.Color(0, 255, 0), strip.Color(0, 0, 255),
      strip.Color(0, 0, 0, 255)};
  for (int c = 0; c < 4; c++) {
    setColor(cols[c]);
    delay(400);
  }
  strip.clear();
  strip.show();
}

// ── State transitions ────────────────────────────────────────────────────────
static void enterArmed() {
  gState = ARMED;
  setSw(false);
  resetAnimation(10);
}

static void enterHit(uint8_t shooterId, uint8_t damage) {
  gState = HIT;
  gHitMs = millis();
  gStateUntil = gHitMs + gSettings.hitTimeMs;

  // Radio first – the sound should start as fast as possible.
  gNow.sendHitReport(shooterId, damage);

  switchPatternStart();
  resetAnimation(80);
  Serial.printf(
      "[HIT] Treffer von Schuetze %u (dmg %u)! hit_time=%u ms, "
      "cooldown=%u ms\n",
      shooterId, damage, gSettings.hitTimeMs, gSettings.cooldownMs);
}

static void enterCooldown() {
  gState = COOLDOWN;
  gStateUntil = millis() + gSettings.cooldownMs;
  setSw(false);
}

// ── Config changed (CFG_WRITE): nothing to re-apply live ────────────────────
// hit_time/cooldown/pattern are read at the next hit; a running hit
// simply finishes with the old values.
static void applyTargetConfig() {}

// ── Self-test hooks (DEBUG_CMD via config box) ──────────────────────────────
static void hookLedTest() { ledTestPattern(); }
static const TargetDebugHooks kDebugHooks = {hookLedTest};

// ── SoftAP firmware update (UPDATE_BEGIN via config box) ────────────────────
// Blocking mode: ESP-NOW goes down, open AP + upload page (shared
// WebUpdateService from infinitag-now-core). Always ends in ESP.restart():
// into the new firmware after a successful upload, into the old one after
// the timeout. An aborted upload cannot boot (boot slot switches only
// after a validated transfer).
static void runUpdateMode(uint8_t minutes) {
  const uint8_t *m = gNow.ownMac();
  char ap[32];
  snprintf(ap, sizeof(ap), "infinitag-tgt-%02X%02X%02X", m[3], m[4], m[5]);
  char ver[16];
  snprintf(ver, sizeof(ver), "%u.%u.%u", TARGET_FW_MAJOR, TARGET_FW_MINOR,
           TARGET_FW_PATCH);
  char label[24];
  snprintf(label, sizeof(label), "Target %02X%02X%02X", m[3], m[4], m[5]);

  // UPDATE_ACK is already queued as an ESP-NOW send – give it a moment
  // before the radio stack is torn down.
  delay(100);

  setSw(false);  // props off during the update

  WebUpdateService upd;
  if (!upd.begin(ap, ver, label, "infinitag-target")) {
    Serial.println("[UPD] SoftAP-Start fehlgeschlagen -> Reboot");
    delay(800);
    ESP.restart();
  }
  Serial.printf("[UPD] Update-Modus: AP %s, http://%s, %u min\n", ap,
                upd.apIp(), minutes);

  const uint32_t deadline = millis() + (uint32_t)minutes * 60000UL;
  bool ledOn = false;
  while (true) {
    upd.loop();

    // Ring pulses blue as the "update mode" signal.
    const bool on = (millis() / 300) % 2 == 0;
    if (on != ledOn) {
      ledOn = on;
      setColor(on ? strip.Color(0, 0, 200) : 0);
    }

    if (upd.updateDone()) {
      setColor(strip.Color(0, 200, 0));
      delay(1500);  // let the result page reach the browser
      ESP.restart();
    }
    if (millis() >= deadline && !upd.uploadActive()) {
      Serial.println("[UPD] Timeout ohne Upload -> Reboot");
      delay(200);
      ESP.restart();
    }
    delay(5);
  }
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000) delay(10);

  Serial.println("\n=== Infinitag Target (Infinitag Now, v0x02) ===");

  // Switches first: everything off, no prop glitches during boot.
  pinMode(SW1_PIN, OUTPUT);
  pinMode(SW_5V_PIN, OUTPUT);
  pinMode(SW_33V_PIN, OUTPUT);
  digitalWrite(SW1_PIN, LOW);
  digitalWrite(SW_5V_PIN, LOW);
  digitalWrite(SW_33V_PIN, LOW);
  Serial.println("[SW]   Schaltausgaenge LOW (GPIO 21/46/48)");

  // LED ring + boot test (also doubles as the DBG_LED pattern).
  strip.begin();
  strip.setBrightness(LED_BRIGHT);
  strip.clear();
  strip.show();
  Serial.printf("[NEO]  %d x SK6812 RGBW auf GPIO%d\n", LED_COUNT, LED_PIN);
  ledTestPattern();

  // Persistent config.
  gSettings.load();
  Serial.printf(
      "[CFG]  snd=%u hit=%ums cd=%ums ani=%u ch=0x%X\n", gSettings.soundId,
      gSettings.hitTimeMs, gSettings.cooldownMs, gSettings.swAnimation,
      gSettings.swChannels);

  // ESP-NOW device logic.
  if (gNow.begin(&gSettings, applyTargetConfig, &kDebugHooks)) {
    const uint8_t *m = gNow.ownMac();
    Serial.printf("[NOW]  ESP-NOW bereit, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                  m[0], m[1], m[2], m[3], m[4], m[5]);
  } else {
    Serial.println("[NOW]  FEHLER: ESP-NOW-Init fehlgeschlagen!");
  }

  // TSOP last: from here on hits can fire.
  pinMode(IR_PIN, INPUT_PULLUP);  // TSOP output is open/high when idle
  gIrLastEdgeUs = micros();
  attachInterrupt(digitalPinToInterrupt(IR_PIN), irIsr, CHANGE);
  Serial.printf("[IR]   TSOP an GPIO%d, Telegramm-Decoder (IrTelegram)\n",
                IR_PIN);

  enterArmed();
  Serial.println("[Setup] Bereit. Treffer = IR-Telegramm der Station.");
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  // ESP-NOW: Discovery/Identify/CFG/Update/Debug.
  gNow.loop();

  // UPDATE_BEGIN received? -> blocking SoftAP update mode (ends in reboot).
  const uint8_t updMin = gNow.consumeUpdateRequest();
  if (updMin != 0) runUpdateMode(updMin);

  // Completed IR telegram from the ISR?
  if (gIrFramePending) {
    gIrFramePending = false;
    const uint16_t frame = gIrFrame;
    uint8_t shooter = 0, damage = 0;
    if (inow::irtDecodeFrame(frame, shooter, damage)) {
      if (gNow.consumeHitTest()) {
        // IR reception test (config box): confirm visually, no hit action.
        setColor(strip.Color(0, 255, 0));
        delay(150);
        resetAnimation(gState == HIT ? 80 : 10);
      } else if (gState == ARMED) {
        Serial.printf("[IR]   Telegramm 0x%04X (Schuetze %u, dmg %u)\n",
                      frame, shooter, damage);
        enterHit(shooter, damage);
      }
      // HIT/COOLDOWN: ignore further telegrams.
    }
  }

  // Phase timing.
  if (gState == HIT && millis() >= gStateUntil) {
    if (gSettings.cooldownMs > 0) {
      enterCooldown();
    } else {
      enterArmed();
    }
  } else if (gState == COOLDOWN && millis() >= gStateUntil) {
    enterArmed();
  }

  // Switch pattern stepping during the hit phase.
  if (gState == HIT) switchPatternTick();

  // LED animation. Identify overrides everything (config box "which
  // device is this?" blink).
  if (gNow.identifyActive()) {
    animationIdentify();
  } else if ((uint32_t)(millis() - gAnimPrevMs) >= gAnimInterval) {
    gAnimPrevMs = millis();
    switch (gState) {
      case HIT:
        animationHit();
        break;
      case COOLDOWN:
        animationCooldown();
        break;
      case ARMED:
      default:
        animationRainbow();
        break;
    }
  }

  delay(2);
}
