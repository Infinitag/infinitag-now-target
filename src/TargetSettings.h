// Persistent target configuration (NVS via Preferences).
// Fields mirror the target config blob, see infinitag-now-core PROTOCOL.md.
// Since protocol v0x02 the target has no user-assigned id anymore – the
// eFuse MAC is the identity.

#pragma once
#include <Arduino.h>

struct TargetSettings {
  uint8_t stationMac[6] = {0};  // station that plays this target's sound
                                //   (all-zero = unset -> hits stay local)
  uint8_t soundId = 1;          // 0-based index into the shared sound catalog
  uint16_t hitTimeMs = 10000;   // hit state: LED wave + switch pattern
  uint16_t cooldownMs = 2000;   // re-arm delay after the hit state
  uint8_t swAnimation = 0;      // switch pattern index (0..1)
  uint8_t swChannels = 0b111;   // bit0=SW1, bit1=SW_5V, bit2=SW_3V3

  bool stationSet() const {
    for (uint8_t b : stationMac)
      if (b != 0) return true;
    return false;
  }

  void load();
  void save() const;
};
