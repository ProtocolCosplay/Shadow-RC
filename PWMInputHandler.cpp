/*
  ╔════════════════════════════════════════════════════════════════════╗
  ║                 PWMInputHandler.cpp - Shadow-RC System             ║
  ║────────────────────────────────────────────────────────────────────║
  ║ This file manages interrupt-based PWM signal reading for joystick  ║
  ║ inputs from both RC controllers. It captures channel pulse widths  ║
  ║ with high accuracy using rising/falling edge detection.            ║
  ║                                                                    ║
  ║ These raw signals are used for drive, turn, and dome motion input. ║
  ║ The functions provided return live PWM values for use in all       ║
  ║ control modes (Manual, Hybrid, Dance, Automated).                  ║
  ║────────────────────────────────────────────────────────────────────║

  FEATURES:
  ─────────────────────────────────────────────────────────────────────
  - Reads 3 critical PWM channels:
      • CH1A (Turn)  – Controller A joystick left/right
      • CH2A (Drive) – Controller A joystick up/down
      • CH1B (Dome)  – Controller B joystick left/right
  - Uses `micros()` with pin-change interrupts for precise timing
  - Live pulse width capture at each loop iteration
  - Configurable input pins for clean physical wiring

  WIRING GUIDANCE:
  ─────────────────────────────────────────────────────────────────────
  - CH1_PIN  (Controller A CH1):     Pin 2
  - CH2_PIN  (Controller A CH2):     Pin 3
  - CH1B_PIN (Controller B CH1):     Pin 21

  • All input pins must be connected to PWM-capable outputs on your RC
    receivers.
  • Pins must support external interrupts — only specific pins on the
    Arduino Mega are interrupt-capable.

  ⚠️  WARNING: DO NOT MODIFY THIS FILE UNLESS ABSOLUTELY NECESSARY ⚠️
  ─────────────────────────────────────────────────────────────────────
  This file directly affects signal processing for all control modes.
  Editing this code may:
     • Break joystick reading entirely
     • Cause false readings or erratic movement
     • Disable dome, drive, or turn logic in your droid

  Only edit this file if:
     • You are reassigning PWM input pins
     • You fully understand Arduino interrupt behavior

  FUNCTION REFERENCE:
  ─────────────────────────────────────────────────────────────────────
  - `setupPWMInputs()`     → Initializes pin modes and interrupts
  - `getPWMValue_CH1A()`   → Returns CH1 (turn) value
  - `getPWMValue_CH2A()`   → Returns CH2 (drive) value
  - `getPWMValue_CH1B()`   → Returns CH1B (dome) value

  INTERNAL USE ONLY:
  - Do not call interrupt handlers manually. They are automatically
    invoked by hardware pin-change events.

  FILE LOCATION:
  ─────────────────────────────────────────────────────────────────────
  This file: `PWMInputHandler.cpp`  
  Header:    `PWMInputHandler.h`

  May the Force be with you, Builder.
  ╚════════════════════════════════════════════════════════════════════╝

*/

#include "PWMInputHandler.h"
#include <Arduino.h>
#include "ComboHandler.h"  // for access to currentMode

// ===================================
// === PIN ASSIGNMENTS (Interrupt) ===
// ===================================
#define CH1_PIN    2   // Channel 1 (Turn - Controller A)
#define CH2_PIN    3   // Channel 2 (Drive - Controller A)
#define CH1B_PIN   21  // Channel 1B (Dome - Controller B)

// ==================================================
// === VOLATILE VALUES (Shared with Interrupts) =====
// ==================================================
volatile int ch1_value  = 1500;  // Turn input (CH1A)
volatile int ch2_value  = 1500;  // Drive input (CH2A)
volatile int ch1b_value = 1500;  // Dome input  (CH1B)

volatile unsigned long ch1_start, ch2_start, ch1b_start;

// ===============================
// === SETUP FUNCTION ===========
// ===============================
void setupPWMInputs() {
  pinMode(CH1_PIN, INPUT);
  pinMode(CH2_PIN, INPUT);
  pinMode(CH1B_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(CH1_PIN), ch1_rise, RISING);
  attachInterrupt(digitalPinToInterrupt(CH2_PIN), ch2_rise, RISING);

  // Only attach CH1B interrupts in Manual or Carpet Mode
  if (currentMode == 1 || currentMode == 4) {
    attachInterrupt(digitalPinToInterrupt(CH1B_PIN), ch1b_rise, RISING);
  } else {
    Serial.println("[PWM] CH1B interrupt skipped for encoder compatibility.");
  }
}

// ===============================
// === ACCESSOR FUNCTIONS ========
// ===============================
int getPWMValue_CH1A()  { return ch1_value;  }
int getPWMValue_CH2A()  { return ch2_value;  }
int getPWMValue_CH1B()  { return ch1b_value; }

// ===============================
// === INTERRUPT HANDLERS ========
// ===============================

// === Channel 1 (Turn) ===
void ch1_rise() {
  ch1_start = micros();
  attachInterrupt(digitalPinToInterrupt(CH1_PIN), ch1_fall, FALLING);
}
void ch1_fall() {
  ch1_value = micros() - ch1_start;
  attachInterrupt(digitalPinToInterrupt(CH1_PIN), ch1_rise, RISING);
}

// === Channel 2 (Drive) ===
void ch2_rise() {
  ch2_start = micros();
  attachInterrupt(digitalPinToInterrupt(CH2_PIN), ch2_fall, FALLING);
}
void ch2_fall() {
  ch2_value = micros() - ch2_start;
  attachInterrupt(digitalPinToInterrupt(CH2_PIN), ch2_rise, RISING);
}

// === Channel 1B (Dome) ===
void ch1b_rise() {
  ch1b_start = micros();
  attachInterrupt(digitalPinToInterrupt(CH1B_PIN), ch1b_fall, FALLING);
}
void ch1b_fall() {
  ch1b_value = micros() - ch1b_start;
  attachInterrupt(digitalPinToInterrupt(CH1B_PIN), ch1b_rise, RISING);
}
