#include "NowTarget.h"

#include "SoundCatalog.h"

using namespace inow;

// All Infinitag-Now devices are pinned to this WiFi channel (PROTOCOL.md).
static constexpr uint8_t ESPNOW_CHANNEL = 1;

// Config validation limits (mirror the config box edit ranges).
static constexpr uint16_t HIT_TIME_MIN_MS = 100;
static constexpr uint16_t HIT_TIME_MAX_MS = 60000;
static constexpr uint16_t COOLDOWN_MAX_MS = 60000;
static constexpr uint8_t SW_ANIMATION_MAX = 1;   // pattern count - 1 (main.cpp)
static constexpr uint8_t SW_CHANNELS_MAX = 0b111;

bool NowTarget::begin(TargetSettings *settings,
                      ConfigChangedFn onConfigChanged,
                      const TargetDebugHooks *hooks) {
  _settings = settings;
  _onConfigChanged = onConfigChanged;
  _hooks = hooks;
  _bootMs = millis();
  return _net.begin(ESPNOW_CHANNEL);
}

void NowTarget::loop() {
  RxPacket rx;
  while (_net.receive(rx)) handlePacket(rx);

  // armed IR reception test timed out?
  if (_hitTestUntil != 0 && millis() >= _hitTestUntil) {
    _hitTestUntil = 0;
    sendDebugResult(_hitTestMac, DBG_TRIGGER, DBG_RES_TIMEOUT);
    Serial.println("[NOW] IR-Empfangs-Test: TIMEOUT");
  }
}

bool NowTarget::sendHitReport() {
  if (!_settings->stationSet()) {
    Serial.println("[NOW] HIT: keine Station konfiguriert - kein Funk");
    return false;
  }
  Packet p;
  init(p, MSG_HIT_REPORT, DEV_TARGET);
  encodeHitReport(_settings->stationMac, _settings->soundId, p.payload);
  const bool ok = _net.sendBroadcast(p);
  const uint8_t *sm = _settings->stationMac;
  Serial.printf("[NOW] HIT_REPORT -> Station %02X%02X%02X, Sound %u (%s)\n",
                sm[3], sm[4], sm[5], _settings->soundId, ok ? "ok" : "FEHLER");
  return ok;
}

uint8_t NowTarget::consumeUpdateRequest() {
  const uint8_t m = _updateReqMin;
  _updateReqMin = 0;
  return m;
}

bool NowTarget::consumeHitTest() {
  if (_hitTestUntil == 0 || millis() >= _hitTestUntil) return false;
  _hitTestUntil = 0;
  sendDebugResult(_hitTestMac, DBG_TRIGGER, DBG_RES_OK);
  Serial.println("[NOW] IR-Empfangs-Test: OK (Treffer erkannt)");
  return true;
}

void NowTarget::sendDebugResult(const uint8_t mac[6], uint8_t test,
                                uint8_t result) {
  Packet p;
  init(p, MSG_DEBUG_RESULT, DEV_TARGET);
  p.payload[0] = test;
  p.payload[1] = result;
  _net.send(mac, p);
}

void NowTarget::handleDebugCmd(const RxPacket &rx) {
  const uint8_t test = rx.pkt.payload[0];
  const uint8_t param = rx.pkt.payload[1];
  Serial.printf("[NOW] DEBUG_CMD: Test %u, Param %u\n", test, param);

  switch (test) {
    case DBG_LED:
      if (_hooks && _hooks->ledTest) {
        _hooks->ledTest();
        sendDebugResult(rx.mac, test, DBG_RES_OK);
      } else {
        sendDebugResult(rx.mac, test, DBG_RES_UNSUPPORTED);
      }
      break;

    case DBG_TRIGGER: {
      // Target semantics: IR reception test. Result is sent when the next
      // IR burst is detected (consumeHitTest) or on timeout in loop().
      const uint8_t s = param == 0 ? 10 : param;
      _hitTestUntil = millis() + (uint32_t)s * 1000UL;
      memcpy(_hitTestMac, rx.mac, 6);
      Serial.printf("[NOW] IR-Empfangs-Test: %u s auf Treffer warten\n", s);
      break;
    }

    default:
      // No sound, laser or IR emitter on the target.
      sendDebugResult(rx.mac, test, DBG_RES_UNSUPPORTED);
      break;
  }
}

void NowTarget::sendDiscoverReply(const uint8_t mac[6], uint8_t token) {
  DiscoverReply r;
  r.fw_major = TARGET_FW_MAJOR;
  r.fw_minor = TARGET_FW_MINOR;
  r.fw_patch = TARGET_FW_PATCH;
  r.rssi_self = 0;  // RSSI not available via the plain recv callback
  r.uptime_min = (uint16_t)((millis() - _bootMs) / 60000UL);

  TargetConfig c;
  memcpy(c.station_mac, _settings->stationMac, 6);
  c.sound_id = _settings->soundId;
  c.hit_time_ms = _settings->hitTimeMs;
  c.cooldown_ms = _settings->cooldownMs;
  c.sw_animation = _settings->swAnimation;
  c.sw_channels = _settings->swChannels;
  r.config_blob_len = TARGET_BLOB_SIZE;
  encodeTargetConfig(c, r.config_blob);

  Packet p;
  init(p, MSG_DISCOVER_REPLY, DEV_TARGET);
  p.token = token;  // echo protection
  encodeDiscoverReply(r, p.payload);
  _net.send(mac, p);
}

void NowTarget::sendAck(const uint8_t mac[6], uint8_t status) {
  Packet p;
  init(p, MSG_CFG_ACK, DEV_TARGET);
  p.payload[0] = status;
  _net.send(mac, p);
}

void NowTarget::handlePacket(const RxPacket &rx) {
  const Packet &p = rx.pkt;

  switch (p.msg_type) {
    case MSG_DISCOVER_REQ: {
      const uint8_t filter = p.payload[0];
      if (filter == DEV_TARGET || filter == DEV_ANY) {
        sendDiscoverReply(rx.mac, p.token);
      }
      break;
    }

    case MSG_IDENTIFY:
      // payload[0] = duration in 100 ms units (self-clearing window)
      _identifyUntil = millis() + (uint32_t)p.payload[0] * 100UL;
      break;

    case MSG_CFG_WRITE: {
      if (p.device_type != DEV_TARGET) break;
      TargetConfig c;
      decodeTargetConfig(p.payload, TARGET_BLOB_SIZE, c);
      if (c.sound_id >= SOUND_COUNT || c.hit_time_ms < HIT_TIME_MIN_MS ||
          c.hit_time_ms > HIT_TIME_MAX_MS || c.cooldown_ms > COOLDOWN_MAX_MS ||
          c.sw_animation > SW_ANIMATION_MAX || c.sw_channels > SW_CHANNELS_MAX) {
        sendAck(rx.mac, ACK_NACK_VALIDATION);
        break;
      }
      memcpy(_settings->stationMac, c.station_mac, 6);
      _settings->soundId = c.sound_id;
      _settings->hitTimeMs = c.hit_time_ms;
      _settings->cooldownMs = c.cooldown_ms;
      _settings->swAnimation = c.sw_animation;
      _settings->swChannels = c.sw_channels;
      _settings->save();
      if (_onConfigChanged) _onConfigChanged();
      sendAck(rx.mac, ACK_OK);
      Serial.printf(
          "[NOW] CFG_WRITE: sta=%02X%02X%02X snd=%u hit=%ums cd=%ums "
          "ani=%u ch=0x%X\n",
          c.station_mac[3], c.station_mac[4], c.station_mac[5], c.sound_id,
          c.hit_time_ms, c.cooldown_ms, c.sw_animation, c.sw_channels);
      break;
    }

    case MSG_UPDATE_BEGIN: {
      // Ack first (the send is queued before we tear ESP-NOW down in main).
      Packet ack;
      init(ack, MSG_UPDATE_ACK, DEV_TARGET);
      ack.payload[0] = 0;
      _net.send(rx.mac, ack);
      _updateReqMin = p.payload[0] == 0 ? 5 : p.payload[0];
      Serial.printf("[NOW] UPDATE_BEGIN: SoftAP-Update-Modus, %u min\n",
                    _updateReqMin);
      break;
    }

    case MSG_DEBUG_CMD:
      if (p.device_type == DEV_TARGET || p.device_type == DEV_ANY)
        handleDebugCmd(rx);
      break;

    // MSG_PUSH_BEGIN/END: ESP-NOW radio push not implemented yet
    // (Doc 21 stage 4) – silently ignored, the box falls back to SoftAP.

    default:
      break;
  }
}
