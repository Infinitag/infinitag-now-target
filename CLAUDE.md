# CLAUDE.md – infinitag-now-target

Firmware der Targets (Ziele) des Infinitag-Now-Systems (Solo-Projekt von
Tobias Stewen; Halloween-Schießbude, infrastrukturloses ESP-NOW-Setup).

## Sprache & Stil

Antworten/Doku Deutsch, Code-Kommentare Englisch. Optionen vorschlagen,
nicht nur ausführen; größere Umbauten erst absprechen.

## Verbindliche Doku (VOR Änderungen lesen!)

- `PROTOCOL.md` im Repo `infinitag-now-core` – Funkprotokoll v0x03
  (inkl. IR-Telegramm-Spezifikation, `IrTelegram.h`).
  **Protokolländerungen nur dort** (Code + Spec + Tests in einem Commit).
- Wissensbasis (Master: `/Volumes/Basteln/Infinitag/wissensbasis/`, Kopie
  in `infinitag-now/docs/`): `04-hardware-target.md` (Target-V3.2-PCB),
  `08-software-target.md` (alte WLAN/HTTP-Firmware, abgelöst),
  `11-offene-punkte.md` Punkt 41 (IR-Schuss: **revidiert 2026-07-24** –
  IR-Telegramm mit Schützen-ID + Schaden + CRC-4 statt Burst-Fenster;
  Treffer-Routing dynamisch, keine Station-Zuordnung mehr).

## Hardware (Target-V3.2-PCB, unverändert übernommen)

| Funktion | GPIO | Anmerkung |
|---|---|---|
| TSOP4138 IR-Empfänger | 15 | OUT active-LOW; Pin-Annahme aus altem Code, beim Bring-up verifizieren! |
| LED-Ring 12× SK6812 RGBW | 38 | NEO_GRBW |
| SW1 (potentialfrei, Optokoppler) | 21 | sw_channels bit0 |
| SW-5V (NPN) | 46 | sw_channels bit1 |
| SW-3.3V (NPN) | 48 | sw_channels bit2 |

## Build

```bash
pio run -t upload      # UART-Flash (erstes Flashen der OTA-FW per Kabel!)
```

Die Core-Lib kommt per `lib_deps`-**Symlink** auf die lokale Arbeitskopie
(`repos/infinitag-now-core`; GitHub-Tag als Alternative in der
`platformio.ini` kommentiert). Arduino-Core 2.x (espressif32@^6.7.0,
alte esp_now-Callback-Signatur) – nicht ungefragt auf 3.x heben.

## Architektur

- `src/main.cpp` – Hardware + Spiellogik: IR-Telegramm-Empfang per
  GPIO-Interrupt (ISR misst Mark/Space-Phasen und füttert den
  `IrtDecoder` aus dem Core; nur CRC-gültige Frames = Treffer),
  Zustandsmaschine ARMED → HIT (Grün-Blitz nach dem HIT_REPORT, dann
  hit_time_ms rote Welle + Switch-Pattern) → COOLDOWN (cooldown_ms:
  rote Welle läuft weiter) → ARMED (Rainbow = wieder scharf).
  Alles non-blocking in `loop()` – **kein** zweiter FreeRTOS-Task mehr
  (die alte Firmware hatte einen; entfällt, weil nichts mehr blockiert
  außer dem Update-Modus).
- `src/NowTarget.*` – ESP-NOW-Gerätelogik v0x03: DISCOVER_REPLY,
  IDENTIFY, CFG_WRITE/CFG_ACK, UPDATE_BEGIN (SoftAP-OTA), DEBUG_CMD
  (DBG_LED = Ringtest, DBG_TRIGGER = IR-Treffer-Test), sendet
  HIT_REPORT (Broadcast: shooter_id aus dem Telegramm, sound_id,
  damage – die Station mit passender ir_id spielt).
- `src/TargetSettings.*` – NVS-Persistenz des Target-Config-Blobs
  (sound_id, hit_time_ms, cooldown_ms, sw_animation, sw_channels;
  **keine** Station-Zuordnung mehr seit v0x03).

Verhaltensregeln:
- HIT_REPORT wird SOFORT beim Treffer gesendet (vor LED/Switch-Start) –
  Latenzbudget IR→Sound < 50 ms (Doc 11 Punkt 41; Telegrammdauer
  22,8–32,4 ms ist eingerechnet).
- DBG_TRIGGER ist beim Target der **IR-Empfangs-Test**: nächstes
  gültiges Telegramm innerhalb des Timeouts → DBG_RES_OK, ohne
  Hit-Aktion (zum Einschießen der Optik).
- `UPDATE_BEGIN` → `runUpdateMode()`: blockierender SoftAP-Updater
  (`WebUpdateService` aus dem Core, AP `infinitag-tgt-<MACSUFFIX>`),
  endet immer in `ESP.restart()`.
- `PUSH_BEGIN` → `runPushReceiveMode()`: blockierender Funk-Update-
  Empfänger (`EspNowPushReceiver` aus dem Core, Doc 21 E4) – Ring
  pulsiert blau, Props aus; endet immer in `ESP.restart()` (Erfolg =
  grün + neue FW, Fehler/30 s Funkstille = rot + alte FW).

## Stand FW 0.1.0 / nächste Schritte

- Komplett neu (löst die WLAN/HTTP-Firmware in
  `PlatformIo/Projects/Infinitag Target` ab). Build grün,
  **auf Hardware ungetestet**. Erster Test (v0x03, Flag-Day mit neuer
  Station-/Config-Box-FW!): UART-Flash → Discovery über Config-Box →
  CFG_WRITE (Sound) → Schuss von der Station → Sound an der Station
  des Schützen.
- Danach ggf. getrennte Switch-Kanal-Muster.

## Git-Workflow: PRs statt Direkt-Commits

- Änderungen laufen über **Feature-Branches + PRs**: Branch `feat/…`,
  `fix/…`, `docs/…`, `refactor/…`, `chore/…`; PR-Titel im gleichen
  Schema (`feat: …`), Template ausfüllen, **genau ein Typ-Label** setzen
  (`enhancement`/`bug`/`documentation`/`refactor`/`chore`/`protocol`).
- **Squash-Merge** auf `main` – der PR-Titel wird der Commit auf main.
- Claude erstellt Branch + PR; **Merge entscheidet Tobias** (oder Claude
  nach explizitem OK).

## Firmware-Versionen & Releases

- **Versionen entstehen BEWUSST, nie automatisch.** Claude zählt
  `TARGET_FW_*` in `src/NowTarget.h` NICHT eigenmächtig hoch, sondern
  schlägt vor, wenn ein Stand release-würdig ist, und fragt nach.
- **Release-Prozess:** Version in `NowTarget.h` erhöhen → committen
  (PR) → Tag pushen: `git tag vX.Y.Z && git push origin vX.Y.Z`.
  **GitHub Actions** (`.github/workflows/release.yml`) prüft Tag ↔
  Quellversion, baut gegen den in `platformio.ini` kommentierten
  Core-Tag (CI-Pin – bei Core-Releases mitpflegen!) und erstellt das
  GitHub-Release mit `infinitag-target-vX.Y.Z.bin` als Download.
  Lokaler Fallback: `bash release.sh`.

## Lizenz

PolyForm Noncommercial 1.0.0 – Tobias Stewen. Kommerzielle Nutzung nur
mit Genehmigung: info@hallow-tech.de.
