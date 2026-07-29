// DMX_Types.h – Contains structs, enums, and global variables shared across files.
// Display.h – Handles text, formatting, and the crossfade progress bar layout on the TFT.
// Storage.h – Manages saving and loading scene configurations to the ESP32's Flash memory.
// DMX_Controller.ino – The primary program file managing hardware, states, inputs, and the crossfade engine.
// Place all four files inside a single folder named DMX_Controller. The Arduino IDE will automatically open them as separate tabs.

//The code runs an interactive state machine with two modes:
// CHANNEL_SELECT Mode: Rotating the encoder cycles through DMX channels 1 to 512. Pressing the button locks in the chosen channel and switches modes.
// VALUE_EDIT Mode: Rotating the encoder edits that channel's DMX payload value between 0 and 255. Pressing the button again stores the value and returns you to channel selection.
// In a third mode: SCENE_MANAGE. You can toggle into this mode by long-pressing the encoder button (holding it for more than 1 second).
// In Scene Manage mode, turning the encoder changes the active scene number from 1 to 8. 
// Clicking the encoder button while on a scene number loads that saved scene's 512-channel configuration from Flash memory. 
// To save your current live adjustments as a scene, simply click the encoder button while in VALUE_EDIT mode; it will automatically save to your active scene index.
// Dynamic Real-Time Crossfading: Instead of forcing values to snap instantly, selecting a new scene stores those targets in a dmxTargetUniverse array. 
// A background rendering scheduler interpolates values smoothly toward the target over a 2.5-second window using a non-blocking timeline tracker.
//"Copy/Paste" Interface Workflow: When inside SCENE_MANAGE mode:Double-click the encoder button to copy the current scene to a clipboard buffer.
//   Turn the dial to find your destination scene slot.
//   Double-click again to paste the clipboard data directly into that slot and save it to flash memory.
// Altering Crossfade Time: Inside SCENE_MANAGE mode, press and hold the button for 1 second to enter DURATION_EDIT sub-mode. Spin the encoder to change the current scene's fade time (in 100ms steps). 
// Click once to lock it and return to the scene manager.
//
// To Move Between Modes: Hold the encoder down for 1 second. The screen will step from Channel Select ➔ Scene Manage ➔ Set Fade Rate ➔ back to Channel Select.
// Commanding a Crossfade vs. An Instant Snap: When choosing a scene to activate in SCENE_MANAGE mode:
//    Single Click: Triggers a smooth crossfade into your highlighted scene and drops you back into channel selection.
//    Double Click: Snaps instantly into your highlighted scene, bypassing the progress bar, and drops you back into channel selection.
//      Inside Edit Duration Mode: Turn the wheel to alter your millisecond fade parameters. Single click to lock in your modifications and save them to flash memory.
// Grand Master Scaling Mode (MASTER_SCALE): Accessible by triple-clicking the encoder button from any mode. In this mode, rotating the encoder scales a global multiplier from 0% to 100%. This downscales all outgoing DMX channel payloads linearly without modifying the values stored in your scenes.
// On-Screen Progress Bar: When a crossfade is active, a graphical progress bar renders across the bottom of the landscape TFT screen (y = 125), showing exactly how much time remains on the transition.

/*
 found a solution for this issue, which is to explicitly add the TX and RX pins to the dmxSerial.begin() function. I got it working just changing this line in the setup:

  dmxSerial.begin(DMX_BAUD, DMX_FORMAT, 16, 17); pin 6 and 5 on the Adafruit ESP32-S3 TFT Feather
Explanation: The SparkFun library for the DMX shield uses UART2, which in the ESP32 Thing Plus has RX in GPIO16 and TX in GPIO17. The library uses the HardwareSerial library to access the UART2 port, which accepts the pin definition as arguments for the begin() function. 
*/
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "Adafruit_seesaw.h"
#include <SparkFunDMX.h>

#include "DMX_Types.h"
#include "Display.h"
#include "Storage.h"

// --- Global Variable Instantiations ---
ControllerState currentState = CHANNEL_SELECT;

uint8_t dmxUniverse[TOTAL_DMX_CHANNELS] = {0};       
uint8_t dmxTargetUniverse[TOTAL_DMX_CHANNELS] = {0}; 
uint8_t dmxClipboard[TOTAL_DMX_CHANNELS] = {0};      
SceneProfile currentSceneConfig;                     

int16_t selectedChannel = 1;      
int16_t selectedScene = 1;        
int16_t grandMasterPercent = 100; 
int32_t lastEncoderPosition = 0;
bool lastButtonState = true;

// Timing Variables
unsigned long lastScrollTime = 0;
unsigned long buttonPressedTime = 0;
unsigned long lastButtonReleaseTime = 0; 
uint8_t clickCount = 0;            
bool isLongPressHandled = false;
bool isClipboardPopulated = false; 

// Crossfade Parameters
unsigned long crossfadeStartTime = 0;
unsigned long activeCrossfadeDuration = 0; 
bool isCrossfading = false;

// --- Class Hardware Objects ---
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_seesaw ss;
SparkFunDMX dmx;
HardwareSerial dmxSerial(1); 
Preferences preferences;     

void setup() {
  Serial.begin(115200);

  // 1. Power up TFT screen and I2C lines
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  // 2. Initialize display hardware
  tft.init(135, 240);
  tft.setRotation(3); 
  tft.fillScreen(ST77XX_BLACK);

  // 3. Initialize STEMMA I2C Encoder
  if (!ss.begin(SEESAW_ADDR)) {
    tft.fillScreen(ST77XX_RED);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 40);
    tft.print("Encoder Error!");
    while (1) delay(10);
  }
  ss.pinMode(SS_SWITCH, INPUT_PULLUP);
  ss.setGPIOInterrupts((uint32_t)1 << SS_SWITCH, 1);
  
  lastEncoderPosition = ss.getEncoderPosition();

  // 4. Initialize SparkFun DMX Shield (250kbps output mode)
  dmxSerial.begin(250000, SERIAL_8N2,RX,TX); // add TX 16 and RX 17 pin numbers
  dmx.begin(dmxSerial, DMX_EN_PIN, TOTAL_DMX_CHANNELS); 
  dmx.setComDir(DMX_WRITE_DIR); 

  // 5. Load preserved setup parameters from boot sector
  preferences.begin("dmx-global", true);
  selectedScene = preferences.getInt("active_scene", 1);
  grandMasterPercent = preferences.getInt("master_lvl", 100);
  preferences.end();
  
  loadSceneFromFlash(selectedScene, false, crossfadeStartTime, activeCrossfadeDuration, isCrossfading); 

  // Static Layout UI
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(10, 15);
  tft.print("DMX CONTROL scn:");
  tft.print(selectedScene);
  updateTFTDisplay(true); 
}

void loop() {
  int32_t currentEncoderPosition = ss.getEncoderPosition();
  bool buttonPressed = ss.digitalRead(SS_SWITCH); 

  if (isCrossfading) {
    processCrossfadeEngine();
  }

  // --- Encoder Rotations ---
  if (currentEncoderPosition != lastEncoderPosition) {
    int32_t rawDelta = currentEncoderPosition - lastEncoderPosition;
    // INVERT DIRECTION: Multiply by -1 so Clockwise increases values
    rawDelta = rawDelta * -1;

    int32_t acceleratedDelta = getAcceleratedDelta(rawDelta);

    if (currentState == CHANNEL_SELECT) {
      selectedChannel += acceleratedDelta;
      if (selectedChannel < 1) selectedChannel = 1;
      if (selectedChannel > 512) selectedChannel = 512;
    } 
    else if (currentState == VALUE_EDIT) {
      int16_t tentativeValue = dmxUniverse[selectedChannel - 1] + acceleratedDelta;
      if (tentativeValue < 0) tentativeValue = 0;
      if (tentativeValue > 255) tentativeValue = 255;
      
      dmxUniverse[selectedChannel - 1] = (uint8_t)tentativeValue;
      dmxTargetUniverse[selectedChannel - 1] = (uint8_t)tentativeValue;
      pushDMXToShield();
    }
    else if (currentState == SCENE_MANAGE) {
      selectedScene += (rawDelta > 0) ? 1 : -1;
      if (selectedScene < 1) selectedScene = 1;
      if (selectedScene > MAX_SCENES) selectedScene = MAX_SCENES;
    }
    else if (currentState == DURATION_EDIT) {
      int32_t tentativeDuration = currentSceneConfig.fadeDurationMS + (rawDelta * 100);
      if (tentativeDuration < 0) tentativeDuration = 0;
      if (tentativeDuration > 20000) tentativeDuration = 20000; 
      currentSceneConfig.fadeDurationMS = (uint16_t)tentativeDuration;
    }
    else if (currentState == MASTER_SCALE) {
      grandMasterPercent += rawDelta; 
      if (grandMasterPercent < 0) grandMasterPercent = 0;
      if (grandMasterPercent > 100) grandMasterPercent = 100;
      pushDMXToShield(); 
    }
    
    updateTFTDisplay(false); 
    lastEncoderPosition = currentEncoderPosition;
  }

  // --- Step B: Long-Press Mode Management Trigger ---
  if (!buttonPressed) { // Button is currently being held down (Active LOW)
    if (buttonPressedTime == 0) {
      buttonPressedTime = millis();
    }
    
    unsigned long duration = millis() - buttonPressedTime;
    
    // Clean 1-Second Hold to cycle menus sequentially
    if (duration > 1000 && !isLongPressHandled && currentState != MASTER_SCALE) {
      if (currentState == CHANNEL_SELECT) {
        currentState = SCENE_MANAGE;
      } else if (currentState == SCENE_MANAGE) {
        currentState = DURATION_EDIT;
      } else if (currentState == DURATION_EDIT) {
        currentState = CHANNEL_SELECT;
      }
      
      isLongPressHandled = true;
      clickCount = 0; // Clear accidental clicks accumulated while holding
      updateTFTDisplay(true);
    }
  }

  // --- Step C: Short-Clicks & Multi-Click Timeout Evaluation ---
  if (buttonPressed != lastButtonState) {
    if (buttonPressed) { // Rising Edge: Button was just released
      unsigned long pressDuration = millis() - buttonPressedTime;
      buttonPressedTime = 0; // Reset tracking clock immediately

      // Only count as a short click if a long press wasn't already triggered
      if (!isLongPressHandled && pressDuration > 20) {
        clickCount++;
        lastButtonReleaseTime = millis();
      }
      
      // Reset long press tracker once physical button is fully released
      isLongPressHandled = false; 
    }
    lastButtonState = buttonPressed;
  }

  // Multi-Click Processing Router (Fires 300ms after the last release)
  if (clickCount > 0 && (millis() - lastButtonReleaseTime > 300)) {
    if (clickCount == 3) {
      // TRIPLE CLICK: Toggle Grand Master Mode globally from any state
      if (currentState == MASTER_SCALE) {
        preferences.begin("dmx-global", false);
        preferences.putInt("master_lvl", grandMasterPercent);
        preferences.end();
        currentState = CHANNEL_SELECT;
      } else {
        currentState = MASTER_SCALE;
      }
    } 
    else if (clickCount == 2) {
      // DOUBLE CLICK PROTOCOLS
      if (currentState == SCENE_MANAGE) {
        // ACTION: Instant Snap Activation (Bypasses Fade Time)
        loadSceneFromFlash(selectedScene, false, crossfadeStartTime, activeCrossfadeDuration, isCrossfading); 
        
        preferences.begin("dmx-global", false);
        preferences.putInt("active_scene", selectedScene);
        preferences.end();
        
        currentState = CHANNEL_SELECT; // Drop back down to main board
      }
    } 
    else if (clickCount == 1) {
      // SINGLE CLICK CONTEXT ROUTING
      if (currentState == CHANNEL_SELECT) {
        currentState = VALUE_EDIT;
      } 
      else if (currentState == VALUE_EDIT) {
        saveActiveSceneToFlash(selectedScene);
        currentState = CHANNEL_SELECT;
      }
      else if (currentState == SCENE_MANAGE) {
        // ACTION: Crossfade Activation (Uses Stored Fade Time)
        loadSceneFromFlash(selectedScene, true, crossfadeStartTime, activeCrossfadeDuration, isCrossfading); 
        
        preferences.begin("dmx-global", false);
        preferences.putInt("active_scene", selectedScene);
        preferences.end();
        
        currentState = CHANNEL_SELECT; // Drop back down to main board
      }
      else if (currentState == DURATION_EDIT) {
        // ACTION: Save newly configured fade time and exit
        saveActiveSceneToFlash(selectedScene);
        currentState = CHANNEL_SELECT;
      }
      else if (currentState == MASTER_SCALE) {
        currentState = CHANNEL_SELECT; 
      }
    }
    clickCount = 0; // Reset counter stack
    updateTFTDisplay(true);
  }


  dmx.update();
  delay(10);
}

// --- Background Crossfade Interpolation Engine ---
void processCrossfadeEngine() {
  unsigned long now = millis();
  unsigned long elapsed = now - crossfadeStartTime;
  
  if (elapsed >= activeCrossfadeDuration || activeCrossfadeDuration == 0) {
    memcpy(dmxUniverse, dmxTargetUniverse, TOTAL_DMX_CHANNELS);
    isCrossfading = false;
    tft.fillRect(10, 130, 220, 4, ST77XX_BLACK); 
  } else {
    float progress = (float)elapsed / (float)activeCrossfadeDuration;
    
    int16_t barWidth = (int16_t)(220 * progress);
    tft.fillRect(10, 130, barWidth, 4, ST77XX_BLUE);
    tft.fillRect(10 + barWidth, 130, 220 - barWidth, 4, ST77XX_DARKGREY);

    for (int i = 0; i < TOTAL_DMX_CHANNELS; i++) {
      int16_t startValue = dmxUniverse[i];
      int16_t targetValue = dmxTargetUniverse[i];
      
      if (startValue != targetValue) {
        dmxUniverse[i] = startValue + (int16_t)((targetValue - startValue) * progress);
      }
    }
  }
  pushDMXToShield();
}

// --- Outgoing Scale Multiplier Generator ---
void pushDMXToShield() {
  for (int i = 0; i < TOTAL_DMX_CHANNELS; i++) {
    // Process real-time scaling map: Active Value * Master Mult% / 100
    uint8_t scaledValue = (uint8_t)((uint32_t)dmxUniverse[i] * grandMasterPercent / 100);
    
    // SparkFun DMX expects: writeByte(uint8_t value, uint16_t channel)
    // DMX channels are 1-indexed, so we use i + 1
    dmx.writeByte(scaledValue, i + 1);
  }
}

// --- Exponential Scrolling Acceleration Engine ---
int32_t getAcceleratedDelta(int32_t rawDelta) {
  unsigned long currentTime = millis();
  unsigned long timeElapsed = currentTime - lastScrollTime;
  lastScrollTime = currentTime;

  if (currentState == SCENE_MANAGE || currentState == DURATION_EDIT || currentState == MASTER_SCALE) return rawDelta; 

  int32_t direction = (rawDelta > 0) ? 1 : -1;
  int32_t absDelta = abs(rawDelta);

  if (timeElapsed < 40) {
    float factor = (40.0 / (float)timeElapsed);
    return direction * (int32_t)(absDelta * factor * factor);
  }
  return rawDelta;
}
