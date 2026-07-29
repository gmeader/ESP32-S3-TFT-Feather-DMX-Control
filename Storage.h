#ifndef STORAGE_H
#define STORAGE_H

#include <Preferences.h>
#include "DMX_Types.h"

extern Preferences preferences;
void pushDMXToShield(); // Defined in main sketch

// Preserves scenes inside dynamic key spaces
void saveActiveSceneToFlash(int sceneNum) {
  preferences.begin("dmx-structs", false);
  char sceneKey[16];
  snprintf(sceneKey, sizeof(sceneKey), "struct%d", sceneNum);
  memcpy(currentSceneConfig.channels, dmxTargetUniverse, TOTAL_DMX_CHANNELS);
  preferences.putBytes(sceneKey, &currentSceneConfig, sizeof(SceneProfile));
  preferences.end();
}

// Loads structured layout values out from system sectors
void loadSceneFromFlash(int sceneNum, bool triggerFade, unsigned long &crossfadeStartTime, unsigned long &activeCrossfadeDuration, bool &isCrossfading) {
  preferences.begin("dmx-structs", false);
  char sceneKey[16];
  snprintf(sceneKey, sizeof(sceneKey), "struct%d", sceneNum);
  int bytesRead = preferences.getBytes(sceneKey, &currentSceneConfig, sizeof(SceneProfile));
  preferences.end();

  if (bytesRead != sizeof(SceneProfile)) {
    currentSceneConfig.fadeDurationMS = 2500; // Fallback default 2.5s
    memset(currentSceneConfig.channels, 0, TOTAL_DMX_CHANNELS);
  }

  memcpy(dmxTargetUniverse, currentSceneConfig.channels, TOTAL_DMX_CHANNELS);
  activeCrossfadeDuration = currentSceneConfig.fadeDurationMS;

  if (triggerFade && activeCrossfadeDuration > 0) {
    crossfadeStartTime = millis();
    isCrossfading = true;
  } else {
    memcpy(dmxUniverse, dmxTargetUniverse, TOTAL_DMX_CHANNELS);
    pushDMXToShield();
    isCrossfading = false;
    tft.fillRect(10, 130, 220, 4, ST77XX_BLACK);
  }
}

#endif
