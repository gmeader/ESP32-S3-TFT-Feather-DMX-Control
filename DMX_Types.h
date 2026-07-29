#ifndef DMX_TYPES_H
#define DMX_TYPES_H

#include <Arduino.h>

// Alias fix for Adafruit GFX color spelling mismatch
#define ST77XX_DARKGREY      0x7BEF // Maps directly to dark gray color bounds

// Hardware Pin Definitions
#define SEESAW_ADDR          0x36
#define SS_SWITCH            24   // Seesaw internal pin for encoder button
#define DMX_EN_PIN           21   // Transmit enable pin (GPIO 21) 
#define DMX_BAUD 250000
#define DMX_FORMAT SERIAL_8N2
#define TOTAL_DMX_CHANNELS   512  
#define MAX_SCENES           8    

// Controller UI State Machine Framework
enum ControllerState { CHANNEL_SELECT, VALUE_EDIT, SCENE_MANAGE, DURATION_EDIT, MASTER_SCALE };
extern ControllerState currentState;

// Custom Structured Storage Block
struct SceneProfile {
  uint16_t fadeDurationMS;                
  uint8_t channels[TOTAL_DMX_CHANNELS];  
};

// Global Array Buffers
extern uint8_t dmxUniverse[TOTAL_DMX_CHANNELS];       
extern uint8_t dmxTargetUniverse[TOTAL_DMX_CHANNELS]; 
extern uint8_t dmxClipboard[TOTAL_DMX_CHANNELS];      
extern SceneProfile currentSceneConfig;                     

// UI Variables
extern int16_t selectedChannel;      
extern int16_t selectedScene;        
extern int16_t grandMasterPercent; 
extern bool isClipboardPopulated; 

#endif
