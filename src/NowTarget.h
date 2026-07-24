// Device-side Infinitag-Now protocol handling for the target (v0x03):
// DISCOVER_REPLY, IDENTIFY, CFG_WRITE/CFG_ACK, UPDATE_BEGIN, DEBUG_CMD
// and outgoing HIT_REPORT (broadcast; routed dynamically by the
// shooter_id decoded from the IR telegram). Uses the shared
// EspNowService from infinitag-now-core. See PROTOCOL.md.

#pragma once
#include <Arduino.h>

#include "EspNowService.h"
#include "InfinitagNow.h"
#include "TargetSettings.h"

// Target firmware version, reported in DISCOVER_REPLY and shown on the
// update page. Bump on every flashed release, the config box compares it.
static constexpr uint8_t TARGET_FW_MAJOR = 0;
static constexpr uint8_t TARGET_FW_MINOR = 1;
static constexpr uint8_t TARGET_FW_PATCH = 0;

// Hardware hooks for the remote self-test (implemented in main.cpp).
// The target has no sound/laser/IR emitter, so only these apply:
struct TargetDebugHooks {
  void (*ledTest)();  // run LED ring test pattern (may block ~2 s)
};

class NowTarget {
 public:
  // onConfigChanged: apply freshly written settings in main (e.g. abort a
  // running hit if hit_time shrank).
  using ConfigChangedFn = void (*)();

  bool begin(TargetSettings *settings, ConfigChangedFn onConfigChanged,
             const TargetDebugHooks *hooks = nullptr);

  // Call every loop() iteration: drains the RX queue, handles timeouts.
  void loop();

  // Broadcast a HIT_REPORT: shooter_id + damage come from the decoded IR
  // telegram, sound_id from the settings. The station whose ir_id matches
  // shooterId plays the sound (PROTOCOL.md v0x03).
  bool sendHitReport(uint8_t shooterId, uint8_t damage);

  // True while an IDENTIFY window is running (LED override: white pulse).
  bool identifyActive() const { return millis() < _identifyUntil; }

  // UPDATE_BEGIN received: returns the requested timeout in minutes exactly
  // once, 0 = nothing pending. Caller must then enter the SoftAP update
  // mode (blocking) and reboot afterwards.
  uint8_t consumeUpdateRequest();

  // IR hit detected while a DBG_TRIGGER test is armed: reports OK to the
  // config box and returns true (main must then NOT run the hit action).
  // For the target DBG_TRIGGER means "IR reception test" – shoot at the
  // target within the timeout to confirm the optics alignment.
  bool consumeHitTest();

  EspNowService *net() { return &_net; }
  const uint8_t *ownMac() { return _net.ownMac(); }

 private:
  void handlePacket(const RxPacket &rx);
  void sendDiscoverReply(const uint8_t mac[6], uint8_t token);
  void sendAck(const uint8_t mac[6], uint8_t status);
  void handleDebugCmd(const RxPacket &rx);
  void sendDebugResult(const uint8_t mac[6], uint8_t test, uint8_t result);

  EspNowService _net;
  TargetSettings *_settings = nullptr;
  ConfigChangedFn _onConfigChanged = nullptr;
  const TargetDebugHooks *_hooks = nullptr;

  // armed DBG_TRIGGER test (= IR reception test on the target)
  uint32_t _hitTestUntil = 0;
  uint8_t _hitTestMac[6] = {0};

  uint32_t _identifyUntil = 0;
  uint8_t _updateReqMin = 0;  // pending UPDATE_BEGIN timeout, 0 = none
  uint32_t _bootMs = 0;
};
