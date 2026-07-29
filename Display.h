#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "DMX_Types.h"

// Instantiate display object
extern Adafruit_ST7789 tft;

// Graphical Rendering Engine Function
void updateTFTDisplay(bool forceAllDataRefresh) {
  static int16_t registeredChannel = -1;
  static int16_t registeredValue = -1;
  static int16_t registeredScene = -1;
  static uint16_t registeredFadeTime = 9999;
  static int16_t registeredMasterVal = -1;
  static bool registeredClipboardState = false;
  static ControllerState registeredState = CHANNEL_SELECT;

  if (forceAllDataRefresh) {
    tft.fillRect(0, 50, 240, 75, ST77XX_BLACK);
    registeredChannel = -1;
    registeredValue = -1;
    registeredScene = -1;
    registeredFadeTime = 9999;
    registeredMasterVal = -1;
    registeredClipboardState = false;
  }

  // 1. Render Title Header & Mode Changes
  if (currentState != registeredState || selectedScene != registeredScene || forceAllDataRefresh) {
    // If scene changed, redraw the top line header to prevent character overlap
    if (selectedScene != registeredScene || forceAllDataRefresh) {
      tft.fillRect(0, 10, 240, 25, ST77XX_BLACK);
      tft.setCursor(10, 15);
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_CYAN);
      tft.print("DMX CONTROL scn:");
      tft.print(selectedScene);
    }

    // Render Mode Context Lines right below the main title
    tft.fillRect(10, 50, 220, 20, ST77XX_BLACK);
    tft.setCursor(10, 50);
    tft.setTextSize(2);
    if (currentState == CHANNEL_SELECT) {
      tft.setTextColor(ST77XX_ORANGE);
      tft.print("MODE: Select Chan");
    } else if (currentState == VALUE_EDIT) {
      tft.setTextColor(ST77XX_GREEN);
      tft.print("MODE: Set Level");
    } else if (currentState == SCENE_MANAGE) {
      tft.setTextColor(ST77XX_MAGENTA);
      tft.print("MODE: Scene Manage");
    } else if (currentState == DURATION_EDIT) {
      tft.setTextColor(ST77XX_YELLOW);
      tft.print("MODE: Set Fade Rate");
    } else if (currentState == MASTER_SCALE) {
      tft.setTextColor(ST77XX_RED);
      tft.print("MODE: Grand Master");
    }
    registeredState = currentState;
  }


  // 2. Render Target Channel Address
  if (selectedChannel != registeredChannel || forceAllDataRefresh) {
    tft.fillRect(10, 80, 220, 20, ST77XX_BLACK);
    tft.setCursor(10, 80);
    tft.setTextSize(2);
    tft.setTextColor((currentState == CHANNEL_SELECT) ? ST77XX_BLUE : ST77XX_WHITE);
    tft.print("Channel: ");
    tft.print(selectedChannel);
    registeredChannel = selectedChannel;
  }

  // 3. Render Bottom Row Contextual Displays
  if (currentState == SCENE_MANAGE) {
    if (selectedScene != registeredScene || isClipboardPopulated != registeredClipboardState || forceAllDataRefresh) {
      tft.fillRect(10, 105, 220, 20, ST77XX_BLACK);
      tft.setCursor(10, 105);
      tft.setTextSize(2);
      if (isClipboardPopulated) {
        tft.setTextColor(ST77XX_RED);
        tft.print("Paste -> [");
      } else {
        tft.setTextColor(ST77XX_MAGENTA);
        tft.print("Load Scene: [");
      }
      tft.print(selectedScene);
      tft.print("]");
      registeredScene = selectedScene;
      registeredClipboardState = isClipboardPopulated;
    }
  } 
  else if (currentState == DURATION_EDIT) {
    if (currentSceneConfig.fadeDurationMS != registeredFadeTime || forceAllDataRefresh) {
      tft.fillRect(10, 105, 220, 20, ST77XX_BLACK);
      tft.setCursor(10, 105);
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_YELLOW);
      tft.print("Time: ");
      tft.print((float)currentSceneConfig.fadeDurationMS / 1000.0, 1);
      tft.print(" sec");
      registeredFadeTime = currentSceneConfig.fadeDurationMS;
    }
  }
  else if (currentState == MASTER_SCALE) {
    if (grandMasterPercent != registeredMasterVal || forceAllDataRefresh) {
      tft.fillRect(10, 105, 220, 20, ST77XX_BLACK);
      tft.setCursor(10, 105);
      tft.setTextSize(2);
      tft.setTextColor(ST77XX_RED);
      tft.print("Master: ");
      tft.print(grandMasterPercent);
      tft.print("%");
      registeredMasterVal = grandMasterPercent;
    }
  }
  else {
    uint8_t currentLevelValue = dmxUniverse[selectedChannel - 1];
    if (currentLevelValue != registeredValue || grandMasterPercent != registeredMasterVal || forceAllDataRefresh) {
      tft.fillRect(10, 105, 220, 20, ST77XX_BLACK);
      tft.setCursor(10, 105);
      tft.setTextSize(2);
      tft.setTextColor((currentState == VALUE_EDIT) ? ST77XX_GREEN : ST77XX_WHITE);
      tft.print("Lvl: ");
      tft.print(currentLevelValue);
      
      if (grandMasterPercent < 100) {
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_RED);
        tft.print(" (M:"); tft.print(grandMasterPercent); tft.print("%)");
      }
      
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(170, 112);
      tft.print("(Scn "); tft.print(selectedScene); tft.print(")");
      registeredValue = currentLevelValue;
      registeredMasterVal = grandMasterPercent;
    }
  }
}

#endif
