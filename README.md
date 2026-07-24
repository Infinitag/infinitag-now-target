# Infinitag Now – Target

**Die Ziele des Zauberstab-Spiels:** Kinder wirken mit einem Zauberstab
Zaubersprüche auf diese Targets – trifft der Zauber, läuft eine rote
Lichtwelle über den LED-Ring, angeschlossene Props schalten los und die
Station spielt den passenden Sound-Effekt. Alles funkt über ESP-NOW,
ganz ohne WLAN-Router, Server oder App – und läuft damit überall, wo
gespielt wird: zum Beispiel als Halloween-Zauberstand im Vorgarten, auf
dem Kindergeburtstag oder der Gartenparty.

![Plattform](https://img.shields.io/badge/Plattform-ESP32--S3-blue)
![Framework](https://img.shields.io/badge/Framework-Arduino%20%2F%20PlatformIO-orange)
![Funk](https://img.shields.io/badge/Funk-ESP--NOW-purple)
![Lizenz](https://img.shields.io/badge/Lizenz-PolyForm%20NC%201.0.0-lightgrey)

<!-- TODO: Hero-Foto eines Targets einfügen:
     ![Infinitag-Target](docs/target.jpg) -->

## Features

- **Robuste Treffer-Erkennung:** Der Zauber ist ein 38-kHz-IR-Telegramm
  mit Schützen-Kennung, Schadenswert und CRC-Prüfsumme – Störlicht und
  Fernbedienungen erzeugen keine gültigen Treffer, und die
  Treffer-Meldung geht **sofort** raus (Latenzbudget IR → Sound
  unter 50 ms, noch vor der ersten LED)
- **Jeder schießt auf alles:** Das Target erkennt am Telegramm, welcher
  Zauberstab getroffen hat, und der Sound spielt automatisch an der
  Station des Schützen – keine feste Zuordnung, beliebig viele
  Stationen und Ziele
- **LED-Ring mit Spielzuständen:** Regenbogen-Lauf = scharf, kurzes
  grünes Aufblitzen beim Treffer, danach rote Welle bis das Target
  wieder scharf ist – man sieht jedem Target von weitem an, was es
  gerade tut
- **Drei Schaltausgänge für Props:** potentialfrei (Optokoppler), 5 V
  und 3,3 V – feuern beim Treffer ein konfigurierbares Muster ab und
  bringen Halloween-Requisiten zum Leben
- **Funkgesteuert:** Discovery, Identify-Blinken, Sound, Trefferzeiten
  und Switch-Muster/-Kanäle stellt die
  [Config-Box](https://github.com/Infinitag/infinitag-now-config) per
  Funk ein; alle Werte sind im Target dauerhaft gespeichert
- **Einschieß-Hilfe:** IR-Empfangs-Test per Funk – der nächste erkannte
  Telegramm meldet „OK", ohne einen Treffer auszulösen (perfekt zum
  Ausrichten der Optik), dazu ein LED-Ringtest
- **Kabellose Updates:** Firmware-Update per Browser über einen
  SoftAP-Update-Modus – ausgelöst von der Config-Box, kein Aufschrauben,
  kein USB-Kabel

## Das Infinitag-Now-System

| Gerät | Repo | Aufgabe |
|---|---|---|
| **Targets** | dieses Repo | IR-Empfänger an den Zielen, melden Treffer per Funk |
| **Station** | [infinitag-now-station](https://github.com/Infinitag/infinitag-now-station) | Sound + Zauberstab (Zauber-Auslösung, Laser, Status-LEDs) |
| **Config-Box** | [infinitag-now-config](https://github.com/Infinitag/infinitag-now-config) | Handheld-Konfigurator: Discovery, Einstellungen, Updates, Live-Monitor |
| Protokoll-Lib | [infinitag-now-core](https://github.com/Infinitag/infinitag-now-core) | Paketformat, `EspNowService`, SoftAP-Updater, `PROTOCOL.md` |
| Doku | [infinitag-now](https://github.com/Infinitag/infinitag-now) | Wissensbasis (Hardware, Protokoll, Konzepte) |

Geräte identifizieren sich allein über ihre MAC-Adresse – auspacken,
einschalten, auf der Config-Box „Neu suchen", fertig. Kein Pairing,
keine ID-Vergabe.

```
Zauberstab ──IR-Telegramm──▶ Target ──HIT_REPORT──▶ Station des Schützen
           (Schützen-ID +      │                     spielt den Sound
            Schaden + CRC)     ├─ LED-Ring: rote Treffer-Welle
                               └─ SW1 / SW-5V / SW-3.3V: Prop-Muster
```

## Hardware

Target-V3.2-PCB: ESP32-S3-WROOM-1 (N8R8), TSOP4138-IR-Empfänger
(GPIO 15), LED-Ring mit 12× SK6812 RGBW (GPIO 38), drei Schaltausgänge –
SW1 potentialfrei über Optokoppler (GPIO 21), SW-5V (GPIO 46), SW-3.3V
(GPIO 48). Details: Doc 04 (PCB) und Doc 22 (Firmware-Konzept) der
[Wissensbasis](https://github.com/Infinitag/infinitag-now).

## Loslegen

```bash
pio run -t upload        # Firmware flashen (UART)
pio device monitor       # Log, 115200 Baud
```

Nur das allererste Flashen braucht USB – danach kommen Updates über
die Luft (siehe unten).

## Updates

Jede Version gibt es als [GitHub-Release](../../releases) mit fertiger
`infinitag-target-vX.Y.Z.bin`. Einspielen ohne Kabel:

1. Config-Box → Target wählen → **„Update (OTA)"**
2. Das Target öffnet ein WLAN `infinitag-tgt-XXXXXX`
3. Mit Laptop/Handy verbinden, `.bin` auf `http://192.168.4.1` hochladen
4. Das Target prüft das Image, startet neu – die Config-Box meldet
   „Update OK" samt neuer Version

Die Upload-Seite zeigt an, welches Gerät man vor sich hat, und lehnt
falsch benannte Firmware-Dateien ab. Ohne Upload startet das Target
nach Ablauf des Update-Fensters automatisch mit der alten Firmware neu.

## Konfiguration

Alles Einstellbare läuft über die Config-Box (Funk, persistiert im NVS
des Targets): Sound, Trefferzeit (`hit_time`), Cooldown, Switch-Muster
und aktive Schaltkanäle. Eine Station-Zuordnung gibt es nicht – der
Treffer-Sound folgt automatisch dem Schützen. Das Funkprotokoll ist
in [`PROTOCOL.md`](https://github.com/Infinitag/infinitag-now-core/blob/main/PROTOCOL.md)
spezifiziert (v0x03, inkl. IR-Telegramm).

## Entwicklung

| Pfad | Inhalt |
|---|---|
| `src/main.cpp` | Hardware + Spiellogik: IR-Telegramm-Decoder (GPIO-Interrupt + `IrtDecoder` aus dem Core), Zustandsmaschine ARMED → HIT → COOLDOWN, LED-Animationen, Schaltausgänge, Update-Modus |
| `src/NowTarget.*` | ESP-NOW-Gerätelogik (Discovery, Identify, CFG, HIT_REPORT, Debug-Tests, Update) |
| `src/TargetSettings.*` | Persistente Einstellungen (NVS) |
| `partitions_8MB.csv` | 2× OTA-Slots für kabellose Updates |

Änderungen laufen über Pull Requests (Squash-Merge, Typ-Label –
Template liegt in `.github/`). Releases entstehen bewusst über
einen Tag-Push: `git tag vX.Y.Z && git push origin vX.Y.Z` – GitHub
Actions baut und veröffentlicht das Release inkl. `.bin` (lokaler
Fallback: `release.sh`).
Protokolländerungen gehören ins Core-Repo. Arduino-Core 2.x ist
bewusst gepinnt (ESP-NOW-Callback-Signatur).

Geplant: Funk-Push-Update (Update direkt über ESP-NOW, ohne
SoftAP-Umweg), getrennte Muster je Schaltkanal.

## Lizenz

[PolyForm Noncommercial 1.0.0](LICENSE) – © 2026 Tobias Stewen.
Kommerzielle Nutzung nur mit Genehmigung: info@hallow-tech.de.
Ursprung des Namens und der Idee: das Lasertag-Projekt
[Infinitag](https://github.com/Infinitag) (2017); Infinitag Now ist eine
komplette Neuentwicklung als Zauberstab-Spiel, entstanden für einen
Halloween-Zauberstand.
