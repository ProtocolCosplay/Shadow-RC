/*
  ╔═══════════════════════════════════════════════════════════════════╗
  ║                R2-D2 Carpet Mode - Shadow-RC System              ║
  ║───────────────────────────────────────────────────────────────────║
  ║ Carpet Mode provides enhanced drive power and responsiveness for ║
  ║ navigating thick carpet, uneven flooring, or high-friction zones.║
  ║ Dome control is fully active. Drive torque is increased to assist║
  ║ with surface resistance, while maintaining safe manual control.  ║
  ║───────────────────────────────────────────────────────────────────║

  FEATURES:
  ────────────────────────────────────────────────────────────────────
  - Controlled drive + turn via Controller A (CH2A, CH1A)
  - Dome motion via Controller B joystick (CH1B)
  - Exponential curve shaping for analog feel with boosted torque
  - Increased drive speed (customizable) for terrain resistance
  - Kill switch (Combo Mode 4) halts motion immediately

  TUNABLE PARAMETERS:
  ────────────────────────────────────────────────────────────────────

  -- Drive + Turn Control --
  - `expoCurve`:
      Exponential shaping of input curves. Lower = more immediate torque,
      higher = softer low-end response. Typical range: 1.0 to 1.5.

  - `speedLimit`:
      Max speed cap for drive/turn (0–127). Higher values help R2 push
      through carpet drag and terrain friction. Recommended: 45–60.

  - `deadZone`:
      Ignores tiny joystick movement (0 = no deadzone).
      Prevents drift or overcorrection from minor stick wiggle.

  - `taperFallRate`:
      Rate of turn deceleration (higher = faster snap to center).
      Helps with tight maneuvering on high-resistance surfaces.

  -- Dome Control --
  - `domeSpeedLimit`:
      Max power for dome motion (0–100), before fineControlMultiplier is applied.
      Allows fast dome response even on soft or dragging surfaces.

  - `domeDeadZone`:
      Prevents small dome input from causing twitching or noise.
      Value in ± joystick units (default: 2).

  - `fineControlMultiplier`:
      Scales dome output to increase responsiveness. Set to 1 for direct control,
      or increase if dome feels slow due to friction.

  - `domeAccelerationRate` / `domeDecelerationRate`:
      These are not actively used in this mode, but available for optional
      smoothing logic if you later decide to add ramp behavior.

  -- Safety Timeout --
  - `motorTimeoutMs`:
      If no command is received for this duration (in ms), all motion stops.
      Helps prevent runaway conditions or stale signal issues.

  DEBUGGING:
  ────────────────────────────────────────────────────────────────────
  If `DEBUG_MODE` is set to true:
    - Serial monitor will print:
        - Raw and final values for drive, turn, and dome motion
        - Real-time behavior inspection

  FILE LOCATION:
  ────────────────────────────────────────────────────────────────────
  This file: `CarpetMode.cpp`
  Header:    `CarpetMode.h`

  May the Force be with you, Builder.
  ╚═══════════════════════════════════════════════════════════════════╝
*/


#include "CarpetMode.h"
#include "MP3Handler.h"
#include "ComboHandler.h"
#include <Arduino.h>
#include "PWMInputHandler.h"
#include <Sabertooth.h>

// ==========================
//       TUNABLE SETTINGS
// ==========================

#define DEBUG_MODE false  // Set to true to enable Serial debugging

// --- Drive Behavior ---
static float expoCurve        = 1.3;   // Shapes input curve (1 = linear, higher = smoother feel)
static int   speedLimit       = 50;    // Max drive/turn speed (0–127)
static int   deadZone         = 2;     // Ignores small joystick input near center

// --- Turn Damping ---
static int   taperFallRate    = 45;    // How quickly turn speed fades when joystick is released

// --- Dome Control ---
static int   domeDeadZone          = 0;     // Ignores minor dome input noise near center
static int   domeAccelerationRate  = 2;     // Placeholder for dome acceleration ramping (not used)
static int   domeDecelerationRate  = 3;     // Placeholder for dome deceleration ramping (not used)
static int   fineControlMultiplier = 2;     // Boosts dome speed when joystick input is strong
static int   domeSpeedLimit        = 100;   // Max allowed dome speed (0–100)
static float domeLeftGain          = 1.00;  // Adjusts dome speed to the left (compensation)
static float domeRightGain         = 1.00;  // Adjusts dome speed to the right (compensation)

// --- Flick Sensitivity ---
static const unsigned long domeFlickMinDuration = 40;  // Minimum flick time in ms to count as valid
static const int domeFlickThreshold = 5;               // Minimum speed of input to trigger flick logic
static const int maxFlickSpeed = 20;                   // Max dome speed allowed during a flick burst

// --- Safety Timeout ---
static const unsigned long motorTimeoutMs = 50;        // Stop motors if no input for this duration (ms)


// ==========================
//       INTERNAL STATE
// ==========================
static int domeInput = 0;
static int currentDomeSpeed = 0;
static int lastSentDomeSpeed = 0;

static int lastDrive = 0;
static int lastTurn  = 0;
static int savedTurnSpeed = 0;

static unsigned long previousDomeMillis = 0;
static unsigned long lastDriveCommandTime = 0;
static unsigned long lastTurnCommandTime  = 0;

static unsigned long domeStartTime = 0;
static bool domeFlickActive = false;

static bool wasTurnInputActive = false;
static bool lastKillState = false;

// ==========================
//     Sabertooth Setup
// ==========================
static Sabertooth ST(128, Serial2);
static Sabertooth domeMotor(129, Serial2);

// ==========================
//    HELPER DECLARATIONS
// ==========================
static int applyExpoCurve(int input, float curve, int limit = speedLimit);
static int taperToZero(int value);

// ==========================
//           SETUP
// ==========================
void setupCarpetMode() {
  if (DEBUG_MODE) {
    Serial.begin(115115);
    Serial.println("=== Carpet Mode Initialized ===");
  }

  setupPWMInputs();
  Serial2.begin(9600);
  delay(100);
  Serial2.write(0xAA);
  delay(10);
}

// ==========================
//         MAIN LOOP
// ==========================
void loopCarpetMode() {
  static unsigned long lastFrameMicros = 0;
  const unsigned long frameIntervalMicros = 5000;

  unsigned long nowMicros = micros();
  if (nowMicros - lastFrameMicros < frameIntervalMicros) return;
  lastFrameMicros = nowMicros;

  unsigned long now = millis();

  // === Read Inputs ===
  int rawTurn  = getPWMValue_CH1A();
  int rawDrive = getPWMValue_CH2A();
  int rawDome  = getPWMValue_CH1B();

  int mappedTurn  = map(constrain(rawTurn,  1000, 2000), 1000, 2000, -127, 127);
  int mappedDrive = map(constrain(rawDrive, 1000, 2000), 1000, 2000, -127, 127);

  int constrainedDome = constrain(rawDome, 1000, 2000);
  if (constrainedDome >= 1500) {
    domeInput = map(constrainedDome, 1500, 2000, 0, 100) * domeRightGain;
  } else {
    domeInput = map(constrainedDome, 1000, 1500, -100, 0) * domeLeftGain;
  }

  // === Apply Deadzones ===
  if (abs(mappedDrive) <= deadZone) mappedDrive = 0;
  if (abs(mappedTurn)  <= deadZone) mappedTurn  = 0;

  if (abs(mappedDrive) > 40) mappedTurn = constrain(mappedTurn, -100, 100);
  if (mappedDrive == 0 && mappedTurn != 0) lastTurn = constrain(lastTurn, -40, 40);

  // === Exponential Curve ===
  int rawCurvedDome = applyExpoCurve(domeInput, expoCurve, domeSpeedLimit);
  int curvedDome = domeFlickActive ? rawCurvedDome : rawCurvedDome * fineControlMultiplier;

  int curvedDrive = applyExpoCurve(mappedDrive, expoCurve);
  int curvedTurn  = applyExpoCurve(mappedTurn,  expoCurve);

  // === Drive / Turn Logic ===
  lastDrive = curvedDrive;
  lastDriveCommandTime = now;

  if (mappedTurn == 0 && wasTurnInputActive) lastTurn = 0;
  else if (mappedTurn == 0) lastTurn = 0;
  else {
    savedTurnSpeed = curvedTurn;
    lastTurn = curvedTurn;
    lastTurnCommandTime = now;
  }

  wasTurnInputActive = (mappedTurn != 0);

  // === Kill Switch (Combo 1) ===
  bool killActive = isComboModeActive(1);

  if (killActive != lastKillState) {
    if (killActive && DEBUG_MODE) Serial.println("[KILL SWITCH ACTIVE]");
    else if (DEBUG_MODE)          Serial.println("[KILL SWITCH RELEASED]");
    lastKillState = killActive;
  }

  if (killActive) {
    lastDrive = 0;
    lastTurn = 0;
    curvedDome = 0;
  }

  // === Dome Logic (with Flick Control) ===
  if (abs(domeInput) < domeDeadZone) {
    currentDomeSpeed = 0;
    if (domeFlickActive && (now - domeStartTime < domeFlickMinDuration)) {
      int limitedFlick = constrain(lastSentDomeSpeed, -maxFlickSpeed, maxFlickSpeed);
      currentDomeSpeed = limitedFlick;
    } else {
      domeFlickActive = false;
    }
  } else {
    currentDomeSpeed = curvedDome;
    if (abs(curvedDome) >= domeFlickThreshold) {
      domeStartTime = now;
      domeFlickActive = true;
    }
  }

  // === Safety Timeout ===
  if (now - lastDriveCommandTime > motorTimeoutMs) lastDrive = 0;
  if (now - lastTurnCommandTime  > motorTimeoutMs) lastTurn  = 0;

  // === Motor Outputs ===
  ST.drive(lastDrive);
  ST.turn(lastTurn);

  if (currentDomeSpeed != lastSentDomeSpeed) {
    domeMotor.motor(currentDomeSpeed);
    previousDomeMillis = now;
    lastSentDomeSpeed = currentDomeSpeed;
  }

  // === Debug Output ===
  if (DEBUG_MODE) {
    Serial.print("DriveRaw: "); Serial.print(mappedDrive);
    Serial.print(" | DriveOut: "); Serial.print(lastDrive);
    Serial.print(" || TurnRaw: "); Serial.print(mappedTurn);
    Serial.print(" | TurnOut: "); Serial.print(lastTurn);
    Serial.print(" || DomeRaw: "); Serial.print(domeInput);
    Serial.print(" | DomeOut: "); Serial.println(currentDomeSpeed);
  }
}

// ==========================
//     HELPER FUNCTIONS
// ==========================

static int applyExpoCurve(int input, float curve, int limit) {
  float normalized = abs(input) / 127.0;
  float curved = pow(normalized, curve) * limit;
  return (input >= 0) ? (int)curved : -(int)curved;
}

static int taperToZero(int value) {
  int taperRate = map(abs(value), 0, speedLimit, 5, taperFallRate);
  if (value > 0) {
    value -= taperRate;
    if (value < 0) value = 0;
  } else if (value < 0) {
    value += taperRate;
    if (value > 0) value = 0;
  }
  return value;
}