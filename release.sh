#!/usr/bin/env bash
# Firmware-Release des Targets: baut, taggt vX.Y.Z, pusht und erstellt
# ein GitHub-Release mit der firmware.bin als Download-Asset.
#
# Aufruf:  bash release.sh
# Version: kommt aus src/NowTarget.h (TARGET_FW_MAJOR/MINOR/PATCH) –
#          vorher BEWUSST hochzaehlen und committen (siehe CLAUDE.md).
set -euo pipefail
cd "$(dirname "$0")"

export PATH="$HOME/.platformio/penv/bin:$PATH"

# --- Version aus den Quellen lesen -------------------------------------------
MAJOR=$(sed -n 's/.*TARGET_FW_MAJOR = \([0-9]*\).*/\1/p' src/NowTarget.h)
MINOR=$(sed -n 's/.*TARGET_FW_MINOR = \([0-9]*\).*/\1/p' src/NowTarget.h)
PATCH=$(sed -n 's/.*TARGET_FW_PATCH = \([0-9]*\).*/\1/p' src/NowTarget.h)
VER="v${MAJOR}.${MINOR}.${PATCH}"
ASSET="infinitag-target-${VER}.bin"

# --- Vorbedingungen -----------------------------------------------------------
if [[ -n "$(git status --porcelain)" ]]; then
  echo "FEHLER: Working tree nicht sauber – erst committen." >&2
  exit 1
fi
# Tag darf existieren, wenn er auf HEAD zeigt (Wiederaufsetzen nach einem
# abgebrochenen Lauf, z. B. Netzwerkfehler beim gh-Aufruf).
if git rev-parse "$VER" >/dev/null 2>&1; then
  if [[ "$(git rev-parse "$VER^{commit}")" != "$(git rev-parse HEAD)" ]]; then
    echo "FEHLER: Tag $VER existiert und zeigt NICHT auf HEAD – erst TARGET_FW_PATCH in src/NowTarget.h erhoehen." >&2
    exit 1
  fi
  echo "Hinweis: Tag $VER existiert schon (HEAD) – setze Release fort."
fi
if gh release view "$VER" >/dev/null 2>&1; then
  echo "FEHLER: Release $VER existiert schon auf GitHub." >&2
  exit 1
fi

# --- Bauen ---------------------------------------------------------------------
echo "== Baue Target $VER =="
pio run
# Build-Ordner liegt wegen workspace_dir (SMB-Workaround) unter ~/.pio-workspaces
BIN="$HOME/.pio-workspaces/infinitag-now-target/build/esp32-s3-devkitc-1/firmware.bin"
[[ -f "$BIN" ]] || BIN=".pio/build/esp32-s3-devkitc-1/firmware.bin"
[[ -f "$BIN" ]] || { echo "FEHLER: firmware.bin nicht gefunden." >&2; exit 1; }
mkdir -p dist
cp "$BIN" "dist/$ASSET"
# CRC32-Beilage: die Box verifiziert damit ihre Internet-Downloads
python3 -c "import zlib;d=open('dist/$ASSET','rb').read();print('%08X %d'%(zlib.crc32(d)&0xffffffff,len(d)))" > "dist/$ASSET.crc32"

# --- Taggen, pushen, Release ---------------------------------------------------
# Release-Notes generiert GitHub aus den gemergten PRs seit dem letzten Tag,
# gruppiert nach Labels (.github/release.yml) – deshalb: Aenderungen ueber
# PRs einbringen, sonst tauchen sie hier nicht auf.
git rev-parse "$VER" >/dev/null 2>&1 || git tag "$VER"
git push origin main --tags
# gh bricht gelegentlich mit fluechtigen Netzwerkfehlern ab ("bad file
# descriptor") -> bis zu 3 Versuche; danach hilft erneutes Ausfuehren des
# Skripts (Tag-auf-HEAD wird oben erkannt und uebersprungen).
created=""
for attempt in 1 2 3; do
  if gh release create "$VER" "dist/$ASSET" "dist/$ASSET.crc32" \
       --title "Target $VER" \
       --generate-notes \
       --notes "**Installation:** Config-Box → Geraete-Menue → Update (OTA), dann \`${ASSET}\` auf http://192.168.4.1 hochladen."; then
    created=1
    break
  fi
  echo "gh release create fehlgeschlagen (Versuch $attempt/3) – neuer Versuch in 5 s ..."
  sleep 5
done
[[ -n "$created" ]] || { echo "FEHLER: Release nicht erstellt – Skript erneut ausfuehren." >&2; exit 1; }

echo "== Release $VER erstellt =="
gh release view "$VER" --json url -q .url
