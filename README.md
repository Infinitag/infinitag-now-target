# infinitag-now-target

Firmware der **Targets** (Ziele) des Infinitag-Now-Systems – ESP-NOW,
ohne WLAN-Router, MQTT oder HTTP. Teil der Halloween-Schießbude von
[Infinitag](https://github.com/Infinitag).

## Funktion

Ein Target erkennt den IR-Schuss der Wand-Station (38-kHz-Burst am
TSOP4138), meldet den Treffer per `HIT_REPORT`-Broadcast an die
konfigurierte Station (die den Sound spielt) und feuert lokal LED-Ring
und Schaltausgänge für Halloween-Props.

```
Wand-Station ──IR-Burst──▶ Target ──ESP-NOW HIT_REPORT──▶ Station spielt Sound
                             │
                             ├─ LED-Ring: rote Treffer-Welle
                             └─ SW1 / SW-5V / SW-3.3V: Prop-Pattern
```

Konfiguriert wird alles über die
[Config-Box](https://github.com/Infinitag/infinitag-now-config):
Discovery, Identify-Blinken, Station-Zuordnung, Sound, Hit-Time,
Cooldown, Switch-Muster/-Kanäle, Selbsttests und Firmware-Update
(SoftAP-OTA). Protokoll: siehe `PROTOCOL.md` im Repo
[infinitag-now-core](https://github.com/Infinitag/infinitag-now-core).

## Hardware

Target-V3.2-PCB: ESP32-S3-WROOM-1 (N8R8), TSOP4138 (GPIO 15), 12× SK6812
RGBW (GPIO 38), drei Schaltausgänge – SW1 potentialfrei über Optokoppler
(GPIO 21), SW-5V (GPIO 46), SW-3.3V (GPIO 48). Details:
`04-hardware-target.md` in der
[Wissensbasis](https://github.com/Infinitag/infinitag-now).

## Build & Flash

```bash
pio run -t upload    # erstes Flashen der OTA-fähigen Firmware per USB!
```

Danach gehen Updates über die Config-Box (Geräte-Menü → Update (OTA),
Upload auf http://192.168.4.1). Releases baut `bash release.sh`.

## Verwandte Repos

| Repo | Inhalt |
| --- | --- |
| [infinitag-now](https://github.com/Infinitag/infinitag-now) | Meta + Doku |
| [infinitag-now-core](https://github.com/Infinitag/infinitag-now-core) | Protokoll-Lib + geteilte Services |
| [infinitag-now-station](https://github.com/Infinitag/infinitag-now-station) | Wand-Station (Sound + IR-Schuss) |
| [infinitag-now-config](https://github.com/Infinitag/infinitag-now-config) | Config-Box |

## Lizenz

PolyForm Noncommercial 1.0.0 – © 2026 Tobias Stewen. Kommerzielle
Nutzung nur mit Genehmigung: info@hallow-tech.de.
