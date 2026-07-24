// Persistent target configuration (NVS via Preferences).
// Fields mirror the target config blob, see infinitag-now-core PROTOCOL.md.
// Since protocol v0x03 there is no station assignment anymore – hits are
// routed by the shooter_id decoded from the IR telegram.

#pragma once
#include <Arduino.h>

struct TargetSettings {
  uint8_t soundId = 1;          // 0-based index into the shared sound catalog
  uint16_t hitTimeMs = 10000;   // hit state: LED wave + switch pattern
  uint16_t cooldownMs = 2000;   // re-arm delay after the hit state
  uint8_t swAnimation = 0;      // switch pattern index (0..1)
  uint8_t swChannels = 0b111;   // bit0=SW1, bit1=SW_5V, bit2=SW_3V3

  void load();
  void save() const;
};
