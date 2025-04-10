/*
  ╔═══════════════════════════════════════════════════════════════════╗
  ║                 R2-D2 Master Control File - Shadow-RC             ║
  ║───────────────────────────────────────────────────────────────────║
  ║ This is the primary entry point for the Shadow-RC control system.║
  ║ It manages mode switching, system initialization, MP3 triggers,  ║
  ║ combo input detection, and kill-switch safety logic.             ║
  ║ This file coordinates all active subsystems and modes.           ║
  ║───────────────────────────────────────────────────────────────────║

  OVERVIEW:
  ────────────────────────────────────────────────────────────────────
  The Shadow-RC system uses dual RC transmitters and an Arduino Mega 
  2560 to control R2-D2. This master file orchestrates control mode 
  transitions, LED indicators, and optional MP3 playback.

  Current supported modes:
    1 = MANUAL_MODE     → Full joystick control over dome + drive
    2 = AUTOMATED_MODE  → Dome motion + MP3s play autonomously
    3 = HYBRID_MODE     → Manual driving + autonomous dome/MP3s
    4 = CARPET_MODE      → More drive power to get across carpeted floors more efficently

  Additional subsystems:
    - ComboHandler: Detects joystick + button combinations
    - MP3Handler: Plays randomized or triggered sounds
    - PWMInputHandler: Maps RC receiver input to usable values

  FEATURES:
  ────────────────────────────────────────────────────────────────────
  - Fully supports optional MarcDuino integration for advanced lighting, 
    sound, and animation — or runs standalone if MarcDuino is not present
  - Mode-based logic that activates only one mode at a time
  - MP3 mode-change confirmation sounds (tracks 001-255)
  - Automatic startup sound (track 255) if MarcDuino is not installed
  - Visual mode indicator via blinking LED (1–4 blinks)
  - Combo system that handles mode switching + advanced features

  TUNABLE BEHAVIOR:
  ────────────────────────────────────────────────────────────────────
  - MP3s can be disabled using `#define DISABLE_MP3`
  - MarcDuino presence can be toggled using `#define MARCDUINO_ENABLED`
  - Mode LED pin is defined as `MODE_STATUS_LED` (defaults to pin 13)

  STRUCTURE:
  ────────────────────────────────────────────────────────────────────
  On startup:
    - Initializes all subsystems
    - Plays startup sound (if no MarcDuino)
    - Enters last used control mode

  In loop():
    - Processes combo inputs
    - Updates MP3 system
    - Handles control mode changes
    - Calls the active mode’s loop
    - Blinks LED to reflect current mode

  DEBUGGING TOOLS:
  ────────────────────────────────────────────────────────────────────
  - Serial output for all mode changes and kill switch events
  - Mode LED for quick visual confirmation (1 blink = Manual, etc.)
  - Startup messages identify detected subsystems (MP3, MarcDuino)

  FILE LOCATION:
  ────────────────────────────────────────────────────────────────────
  This file: `Shadow_RC_Master.ino` (or similar)
  Related headers: `ManualMode.h`, `ComboHandler.h`, etc.

  NOTES:
  ────────────────────────────────────────────────────────────────────
  - Be sure all pins are assigned properly in your PWM handler.
  - Confirm that Serial1 (TX1) is wired to MP3 trigger or MarcDuino.
  - Sabertooth + SyRen must share Serial2 (TX2) with sync byte on boot.

  May the Force be with you, Builder.
  ╚═══════════════════════════════════════════════════════════════════╝

*/

#include "ManualMode.h"
#include "CarpetMode.h"
#include "HybridMode.h"
#include "AutomatedMode.h"

#include "ComboHandler.h"
#include "PWMInputHandler.h"
#include "MP3Handler.h"

// =========================================
// === MODE ENUMERATION ====================
// =========================================
enum Mode {
  MANUAL_MODE     = 1,
  AUTOMATED_MODE  = 2,
  HYBRID_MODE     = 3,
  CARPET_MODE      = 4
};

// =========================================
// === MODE STATUS LED =====================
// =========================================
// This LED blinks to indicate the active mode.
// 1 blink = Manual, 2 = Automated, 3 = Hybrid, 4 = Carpet
#define MODE_STATUS_LED LED_BUILTIN  // Usually pin 13 on Arduino Mega

// =========================================
// === LED BLINK STATE (Non-blocking) ======
// =========================================
unsigned long lastBlinkTime = 0;
int blinkCount = 0;
bool ledState = false;
unsigned long blinkDelay = 150;
unsigned long cycleDelay = 2000;
int currentBlinkTotal = 0;

// =========================================
// === LED PATTERN HANDLER =================
// =========================================
void updateLEDPattern(int mode) {
  unsigned long now = millis();

  // Wait between blink cycles
  if (blinkCount >= currentBlinkTotal && (now - lastBlinkTime >= cycleDelay)) {
    blinkCount = 0;
    currentBlinkTotal = mode;
    lastBlinkTime = now;
    ledState = false;
    digitalWrite(MODE_STATUS_LED, LOW);
  }

  // Blink ON/OFF sequence
  if (blinkCount < currentBlinkTotal && (now - lastBlinkTime >= blinkDelay)) {
    ledState = !ledState;
    digitalWrite(MODE_STATUS_LED, ledState);
    if (!ledState) blinkCount++;  // Count only OFFs
    lastBlinkTime = now;
  }
}

// =========================================
// === SETUP ===============================
// =========================================
void setup() {
  Serial.begin(115200);
  Serial.println("=== R2-D2 Control Master File ===");

  Serial1.begin(9600);  // Serial1 = Sabertooth & SyRen shared TX

  setupPWMInputs();
  setupComboHandler();
  setupMP3Handler();

  pinMode(MODE_STATUS_LED, OUTPUT);
  digitalWrite(MODE_STATUS_LED, LOW);

  delay(1500);  // Let everything initialize

  // === Play startup sound (if no MarcDuino is present) ===
#if !MARCDUINO_ENABLED
  Serial.println(">> No MarcDuino detected — playing startup sound (track 255).");
  mp3.trigger(255);  
#else
  Serial.println(">> MarcDuino enabled — skipping manual startup sound.");
#endif

  // === Initialize current mode ===
  switch (currentMode) {
    case MANUAL_MODE:     setupManualMode();     break;
    case CARPET_MODE:      setupCarpetMode();      break;
    case HYBRID_MODE:     setupHybridMode();     break;
    case AUTOMATED_MODE:  setupAutomatedMode();  break;
  }

  currentBlinkTotal = currentMode;
  lastMode = currentMode;
}

// =========================================
// === MAIN LOOP ===========================
// =========================================
void loop() {
  updateComboHandler();   // Detect joystick+button combos
  updateMP3Handler();     // Process MP3 triggers (if enabled)

  // === Mode Transition Handling ===
  if (currentMode != lastMode) {
    delay(1500);  // Allow systems to stabilize

    switch (currentMode) {
      case MANUAL_MODE:
        Serial.println("==> Switching to MANUAL MODE");
        setupManualMode();
#ifndef DISABLE_MP3
        mp3.trigger(231);  
#endif
        break;

      case CARPET_MODE:
        Serial.println("==> Switching to CARPET MODE");
        setupCarpetMode();
#ifndef DISABLE_MP3
        mp3.trigger(232);  
#endif
        break;

      case HYBRID_MODE:
        Serial.println("==> Switching to HYBRID MODE");
        setupHybridMode();
#ifndef DISABLE_MP3
        mp3.trigger(233);  
#endif
        break;

      case AUTOMATED_MODE:
        Serial.println("==> Switching to AUTOMATED MODE");
        setupAutomatedMode();
#ifndef DISABLE_MP3
        mp3.trigger(234);  
#endif
        break;
    }

    currentBlinkTotal = currentMode;  // Update blink count
    blinkCount = currentBlinkTotal;   // Force pause before next cycle
    lastMode = currentMode;
  }

  // === Call Active Mode Logic ===
  switch (currentMode) {
    case MANUAL_MODE:     loopManualMode();     break;
    case CARPET_MODE:      loopCarpetMode();      break;
    case HYBRID_MODE:     loopHybridMode();     break;
    case AUTOMATED_MODE:  loopAutomatedMode();  break;
  }

  updateLEDPattern(currentMode);  // Visual mode feedback
}
