#include "TargetSettings.h"

#include <Preferences.h>

static const char *NVS_NAMESPACE = "inow-target";

void TargetSettings::load() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);
  soundId = prefs.getUChar("sound", 1);
  hitTimeMs = prefs.getUShort("hittime", 10000);
  cooldownMs = prefs.getUShort("cooldn", 2000);
  swAnimation = prefs.getUChar("swani", 0);
  swChannels = prefs.getUChar("swchan", 0b111);
  prefs.end();
}

void TargetSettings::save() const {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
  prefs.putUChar("sound", soundId);
  prefs.putUShort("hittime", hitTimeMs);
  prefs.putUShort("cooldn", cooldownMs);
  prefs.putUChar("swani", swAnimation);
  prefs.putUChar("swchan", swChannels);
  prefs.end();
}
