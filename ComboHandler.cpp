/*
  ╔════════════════════════════════════════════════════════════════════╗
  ║                  ComboHandler.cpp - Shadow-RC System               ║
  ║────────────────────────────────────────────────────────────────────║
  ║ This file manages all combo-based input logic. It allows the       ║
  ║ builder to trigger up to 32 distinct actions using dual RC         ║
  ║ transmitters, based on joystick + button combinations.             ║
  ║                                                                    ║
  ║ It handles mode switching, MarcDuino commands, sound triggers,     ║
  ║ and safety features such as combo-based kill switches.             ║
  ║────────────────────────────────────────────────────────────────────║

  OVERVIEW:
  ─────────────────────────────────────────────────────────────────────
  The combo system tracks momentary input combinations from:
    • Joystick A (Controller A) + Buttons on Controller B
    • Joystick B (Controller B) + Buttons on Controller A

  This enables advanced control using only RC gear. Combos are
  used to:
    - Switch between control modes (Manual, Carpet, Hybrid, Auto)
    - Send MarcDuino serial commands (Awake+, Quiet, etc.)
    - Trigger MP3 or servo actions (depending on builder setup)
    - Engage safety lockouts or kill switches for motors

  SUPPORTED COMBOS:
  ─────────────────────────────────────────────────────────────────────
  - Combos 1–16: Joystick B + Controller A buttons (CH3–CH6)
  - Combos 17–32: Joystick A + Controller B buttons (CH3–CH6)

  Each combo activates when:
    1. A joystick is held in a valid direction (up/down/left/right)
    2. A momentary button is pressed at the same time on the opposite controller

  FEATURES:
  ─────────────────────────────────────────────────────────────────────
  - 32 total combo slots for full feature mapping
  - Includes support for MarcDuino serial commands
  - Dynamically sets `currentCombo` and `currentMode`
  - Prevents MP3/autonomous actions while a combo is active
  - Combo-based kill switch support for each mode (1–4)
  - Combo timing is optimized for short PWM signal pulses (~1988 µs)

  DEBUGGING:
  ─────────────────────────────────────────────────────────────────────
  Serial monitor will show:
    - Active combo number (1–32)
    - Mode transitions via combo
    - Combo kill switch activation per mode
    - Edge detection feedback for combo timing issues

  ⚠️  WARNING: DO NOT EDIT UNLESS YOU KNOW WHAT YOU ARE DOING  ⚠️
  ─────────────────────────────────────────────────────────────────────
  This file contains sensitive timing logic and combo interpretation.
  Editing this code may:
     • Break mode switching
     • Prevent combos from being detected
     • Cause MP3s or motors to behave incorrectly
     • Lock you into the wrong operational mode

  Only edit this file if:
     • You are adding new combo features AND
     • You fully understand the PWM reading system and combo mapping

  FILE LOCATION:
  ─────────────────────────────────────────────────────────────────────
  This file: `ComboHandler.cpp`  
  Header:    `ComboHandler.h`

  May the Force be with you, Builder.
  ╚════════════════════════════════════════════════════════════════════╝

*/

#include "ComboHandler.h"

// --- External functions from MP3Handler ---
void disableMP3Triggers();
void enableMP3Triggers();

// ---------- MarcDuino Setup ----------
#define MARCDUINO_SETUP 2
#define MARCDUINO_ENABLED     (MARCDUINO_SETUP > 0)
#define MARCDUINO_USE_SERIAL3 (MARCDUINO_SETUP == 2)

// ---------- Controller A ----------
#define RECEIVER_A_CH1_PIN  2
#define RECEIVER_A_CH2_PIN  3
#define CH3_PIN             22
#define CH4_PIN             24
#define CH5_PIN             26
#define CH6_PIN             28

// ---------- Controller B ----------
#define RECEIVER_B_CH1_PIN  21
#define CH2B_PIN            23
#define RECEIVER_B_CH3_PIN  25
#define RECEIVER_B_CH4_PIN  27
#define RECEIVER_B_CH5_PIN  29
#define RECEIVER_B_CH6_PIN  31

// ---------- Thresholds ----------
const int HIGH_THRESHOLD    = 1700;
const int LOW_THRESHOLD     = 1300;
const int COMBO_DOWN_MIN    = 900;
const int COMBO_DOWN_MAX    = 1300;
const int COMBO_UP_MIN      = 1700;
const int COMBO_UP_MAX      = 2100;
const int COMBO_LEFT_MAX    = 1300;
const int COMBO_RIGHT_MIN   = 1700;

// ---------- State Tracking ----------
int last_CH3 = -1, last_CH4 = -1, last_CH5 = -1;
int last_CH3B = -1, last_CH4B = -1, last_CH5B = -1;

bool hasTriggered_CH6_A_DOWN = false, hasTriggered_CH6_A_UP = false, hasTriggered_CH6_A_LEFT = false, hasTriggered_CH6_A_RIGHT = false;
bool hasTriggered_CH6_B_DOWN = false, hasTriggered_CH6_B_UP = false, hasTriggered_CH6_B_LEFT = false, hasTriggered_CH6_B_RIGHT = false;

// ---------- Combo Tracking ----------
int currentCombo = 0;
int currentMode = 1;
int lastCombo = 0;
int lastMode = 0;
unsigned long comboTimestamp = 0;
const unsigned long comboResetDelay = 1000;

// ---------- MarcDuino Trigger ----------
void triggerMarcDuinoSequence(const char* command, int combo, const char* label) {
#if MARCDUINO_ENABLED
  if (MARCDUINO_USE_SERIAL3) Serial3.print(command);
  else Serial1.print(command);
#endif
  Serial.print(">> MarcDuino Trigger: ");
  Serial.print(label);
  Serial.print(" | Combo ");
  Serial.println(combo);
}

// ---------- Setup ----------
void setupComboHandler() {
  pinMode(RECEIVER_A_CH1_PIN, INPUT);
  pinMode(RECEIVER_A_CH2_PIN, INPUT);
  pinMode(CH3_PIN, INPUT);
  pinMode(CH4_PIN, INPUT);
  pinMode(CH5_PIN, INPUT);
  pinMode(CH6_PIN, INPUT);

  pinMode(RECEIVER_B_CH1_PIN, INPUT);
  pinMode(CH2B_PIN, INPUT);
  pinMode(RECEIVER_B_CH3_PIN, INPUT);
  pinMode(RECEIVER_B_CH4_PIN, INPUT);
  pinMode(RECEIVER_B_CH5_PIN, INPUT);
  pinMode(RECEIVER_B_CH6_PIN, INPUT);
}

// ---------- Loop ----------
void updateComboHandler() {
  int ch1a_pwm = pulseIn(RECEIVER_A_CH1_PIN, HIGH, 50000);
  int ch2a_pwm = pulseIn(RECEIVER_A_CH2_PIN, HIGH, 50000);
  int ch1b_pwm = pulseIn(RECEIVER_B_CH1_PIN, HIGH, 50000);
  int ch2b_pwm = pulseIn(CH2B_PIN, HIGH, 50000);

  bool comboDown  = (ch2a_pwm >= COMBO_DOWN_MIN && ch2a_pwm <= COMBO_DOWN_MAX) || (ch2b_pwm >= COMBO_DOWN_MIN && ch2b_pwm <= COMBO_DOWN_MAX);
  bool comboUp    = (ch2a_pwm >= COMBO_UP_MIN && ch2a_pwm <= COMBO_UP_MAX) || (ch2b_pwm >= COMBO_UP_MIN && ch2b_pwm <= COMBO_UP_MAX);
  bool comboLeft  = (ch1a_pwm > 0 && ch1a_pwm <= COMBO_LEFT_MAX) || (ch1b_pwm > 0 && ch1b_pwm <= COMBO_LEFT_MAX);
  bool comboRight = (ch1a_pwm > 0 && ch1a_pwm >= COMBO_RIGHT_MIN) || (ch1b_pwm > 0 && ch1b_pwm >= COMBO_RIGHT_MIN);

  // Joystick B + Controller A buttons
  detectToggleCombo(CH3_PIN, last_CH3, 1, comboDown, comboUp, comboLeft, comboRight);
  detectToggleCombo(CH4_PIN, last_CH4, 2, comboDown, comboUp, comboLeft, comboRight);
  detectToggleCombo(CH5_PIN, last_CH5, 3, comboDown, comboUp, comboLeft, comboRight);
  detectMomentaryCombo(CH6_PIN, hasTriggered_CH6_A_DOWN, hasTriggered_CH6_A_UP,
                       hasTriggered_CH6_A_LEFT, hasTriggered_CH6_A_RIGHT,
                       4, comboDown, comboUp, comboLeft, comboRight);

  // Joystick A + Controller B buttons
  detectToggleCombo(RECEIVER_B_CH3_PIN, last_CH3B, 17, comboDown, comboUp, comboLeft, comboRight);
  detectToggleCombo(RECEIVER_B_CH4_PIN, last_CH4B, 18, comboDown, comboUp, comboLeft, comboRight);
  detectToggleCombo(RECEIVER_B_CH5_PIN, last_CH5B, 19, comboDown, comboUp, comboLeft, comboRight);
  detectMomentaryCombo(RECEIVER_B_CH6_PIN, hasTriggered_CH6_B_DOWN, hasTriggered_CH6_B_UP,
                       hasTriggered_CH6_B_LEFT, hasTriggered_CH6_B_RIGHT,
                       20, comboDown, comboUp, comboLeft, comboRight);

  // Mode Switching
  if (currentCombo >= 1 && currentCombo <= 4 && currentCombo != currentMode) {
    currentMode = currentCombo;
    currentCombo = 0;
  }

  // Debug Mode Print
  if (currentMode != lastMode) {
    switch (currentMode) {
      case 1: Serial.println(">> currentMode: MANUAL MODE"); break;
      case 2: Serial.println(">> currentMode: AUTOMATED MODE"); break;
      case 3: Serial.println(">> currentMode: HYBRID MODE"); break;
      case 4: Serial.println(">> currentMode: CARPET MODE"); break;
    }
    lastMode = currentMode;
  }

  // Handle Combos
  if (currentCombo != lastCombo && currentCombo > 4) {
    if ((currentMode == 1 || currentMode == 4) && currentCombo <= 8 ||
        currentMode == 3 && currentCombo <= 16 ||
        currentMode == 2) {

      Serial.print(">> currentCombo: ");
      Serial.println(currentCombo);

      switch (currentCombo) {
        case 5: triggerMarcDuinoSequence(":SE03\r", 5, "Awake+"); disableMP3Triggers(); break;
        case 6:
          #if MARCDUINO_ENABLED
            if (MARCDUINO_USE_SERIAL3) Serial3.print(":SE00\r");
            else Serial1.print(":SE00\r");
          #endif
          enableMP3Triggers();
          break;
        case 7: triggerMarcDuinoSequence(":SE02\r", 7, "Full Awake"); disableMP3Triggers(); break;
        case 8: triggerMarcDuinoSequence(":SE01\r", 8, "Mid Awake"); disableMP3Triggers(); break;
        case 9: triggerMarcDuinoSequence(":SE10\r", 9, "Leia Message"); break;
        case 10: triggerMarcDuinoSequence(":SE06\r", 10, "Scream"); break;
      }

      comboTimestamp = millis();
    } else {
      currentCombo = 0;
    }
    lastCombo = currentCombo;
  }

  if (currentCombo > 4 && millis() - comboTimestamp > comboResetDelay) {
    currentCombo = 0;
    Serial.println(">> currentCombo: 0");
  }
}

// ---------- Combo Trigger Handlers ----------
void detectToggleCombo(int pin, int &lastState, int baseCombo,
                       bool down, bool up, bool left, bool right) {
  int pwm = pulseIn(pin, HIGH, 50000);
  if (pwm > 0) {
    int state = (pwm > HIGH_THRESHOLD) ? HIGH : LOW;
    if (state != lastState) {
      if (down)       currentCombo = baseCombo;
      else if (up)    currentCombo = baseCombo + 4;
      else if (left)  currentCombo = baseCombo + 8;
      else if (right) currentCombo = baseCombo + 12;
      lastState = state;
    }
  }
}

void detectMomentaryCombo(int pin,
                          bool &trigDown, bool &trigUp, bool &trigLeft, bool &trigRight,
                          int baseCombo,
                          bool down, bool up, bool left, bool right) {
  int pwm = pulseIn(pin, HIGH, 50000);
  if (pwm > 0) {
    if (down && pwm >= 1900 && !trigDown) {
      currentCombo = baseCombo;
      trigDown = true;
    }
    if (up && pwm >= 1900 && !trigUp) {
      currentCombo = baseCombo + 4;
      trigUp = true;
    }
    if (left && pwm >= 1900 && !trigLeft) {
      currentCombo = baseCombo + 8;
      trigLeft = true;
    }
    if (right && pwm >= 1900 && !trigRight) {
      currentCombo = baseCombo + 12;
      trigRight = true;
    }
    if (pwm < LOW_THRESHOLD) {
      trigDown = trigUp = trigLeft = trigRight = false;
    }
  }
}

// ---------- External Access for Kill Switch ----------
bool isComboModeActive(int mode) {
  int ch1a = pulseIn(RECEIVER_A_CH1_PIN, HIGH, 30000);
  int ch2a = pulseIn(RECEIVER_A_CH2_PIN, HIGH, 30000);
  int ch1b = pulseIn(RECEIVER_B_CH1_PIN, HIGH, 30000);
  int ch2b = pulseIn(CH2B_PIN, HIGH, 30000);

  if (mode == 1) {
    return (ch2b >= COMBO_DOWN_MIN && ch2b <= COMBO_DOWN_MAX) ||
           (ch2b >= COMBO_UP_MIN   && ch2b <= COMBO_UP_MAX);
  }

  if (mode == 2) {
    return
      (ch2a >= COMBO_DOWN_MIN && ch2a <= COMBO_DOWN_MAX) ||
      (ch2a >= COMBO_UP_MIN   && ch2a <= COMBO_UP_MAX)   ||
      (ch1a > 0 && ch1a <= COMBO_LEFT_MAX)              ||
      (ch1a > 0 && ch1a >= COMBO_RIGHT_MIN)             ||
      (ch2b >= COMBO_DOWN_MIN && ch2b <= COMBO_DOWN_MAX) ||
      (ch2b >= COMBO_UP_MIN   && ch2b <= COMBO_UP_MAX)   ||
      (ch1b > 0 && ch1b <= COMBO_LEFT_MAX)              ||
      (ch1b > 0 && ch1b >= COMBO_RIGHT_MIN);
  }

  if (mode == 3) {
    return
      (ch2b >= COMBO_DOWN_MIN && ch2b <= COMBO_DOWN_MAX) ||
      (ch2b >= COMBO_UP_MIN   && ch2b <= COMBO_UP_MAX)   ||
      (ch1b > 0 && ch1b <= COMBO_LEFT_MAX)              ||
      (ch1b > 0 && ch1b >= COMBO_RIGHT_MIN);
  }

  if (mode == 4) {
    return
      (ch2b >= COMBO_DOWN_MIN && ch2b <= COMBO_DOWN_MAX) ||
      (ch1b > 0 && ch1b <= COMBO_LEFT_MAX)              ||
      (ch1b > 0 && ch1b >= COMBO_RIGHT_MIN);
  }

  return false;
}
