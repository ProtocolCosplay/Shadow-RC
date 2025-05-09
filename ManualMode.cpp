/*
  ╔════════════════════════════════════════════════════════════════════╗
  ║                  R2-D2 Manual Mode - Shadow-RC System             ║
  ║────────────────────────────────────────────────────────────────────║
  ║ This mode gives the builder full manual control of R2-D2 using    ║
  ║ joystick input from Controller A (drive) and Controller B (dome). ║
  ║ Motion is smooth and responsive with exponential curves, deadzone ║
  ║ filtering, optional turn damping, and dome acceleration.          ║
  ║────────────────────────────────────────────────────────────────────║

  FEATURES:
  ─────────────────────────────────────────────────────────────────────
  - CH1A (Left/Right)  = Turn control (Sabertooth turn mode)
  - CH2A (Up/Down)     = Forward/backward drive
  - CH1B (Left/Right)  = Dome rotation (SyRen)
  - Kill switch combo  = Fully disables both drive and dome motion

  TUNABLE PARAMETERS:
  ─────────────────────────────────────────────────────────────────────
  These variables allow you to customize how Manual Mode behaves. 
  All tuning is done in the "TUNABLE SETTINGS" section of this file.

  - `expoCurve`: 
      Applies a non-linear exponential response to all joystick input.
      Higher values (e.g. 3.0) make small joystick movements gentler,
      while still allowing full speed at full stick. Use 1.0 for linear.

  - `speedLimit`: 
      Caps the maximum drive and turn power. Range: 0–127.
      A value of ~89 is ~70% power, which is safer for testing.

  - `deadZone`: 
      Ignores small joystick movements around the center.
      Prevents twitching or drifting when stick is not perfectly centered.

  - `taperFallRate`: 
      Controls how quickly turning ramps down when the joystick is released.
      Higher values = faster snap-to-zero. Lower = slower coast.

  - `domeDeadZone`: 
      Same as `deadZone`, but for dome control.
      Prevents dome jitter from small stick offsets.

  - `domeAccelerationRate` and `domeDecelerationRate`: 
      Control how quickly the dome ramps up and slows down.
      Higher numbers = smoother, more realistic dome movement.

  - `fineControlMultiplier`: 
      Amplifies dome joystick sensitivity after exponential curve.
      Useful if dome feels sluggish. Try values like 1–3.

  - `motorTimeoutMs`: 
      Safety timeout that stops drive and turn if no input is seen.
      Set in milliseconds. Helps avoid runaway if signal is lost.

  DEBUGGING:
  ─────────────────────────────────────────────────────────────────────
  If `DEBUG_MODE` is set to `true`, the Serial Monitor will show:

  - Raw and curved joystick values
  - Output drive and turn values
  - Dome input and actual dome speed
  - Kill switch activation status

  FILE LOCATION:
  ─────────────────────────────────────────────────────────────────────
  This file:    `ManualMode.cpp`
  Header file:  `ManualMode.h`

  All tuning is done inside this file under "TUNABLE SETTINGS".

  ╚════════════════════════════════════════════════════════════════════╝
*/


#include "ManualMode.h"
#include "MP3Handler.h"
#include "ComboHandler.h"
#include <Arduino.h>
#include "PWMInputHandler.h"
#include <Sabertooth.h>

// ==========================
//       TUNABLE SETTINGS
// ==========================
#define DEBUG_MODE true

static float expoCurve        = 1;
static int   speedLimit       = 25;
static int   deadZone         = 0;
static int   taperFallRate    = 60;

static int   domeDeadZone          = 0;
static int   fineControlMultiplier = 2;
static int   domeSpeedLimit        = 100;
static float domeLeftGain          = 1.00;
static float domeRightGain         = 1.00;

static const unsigned long domeFlickMinDuration = 40;
static const int domeFlickThreshold = 5;
static const int maxFlickSpeed = 20;
static const unsigned long motorTimeoutMs = 50;

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
Sabertooth ST(128, Serial2);
Sabertooth domeMotor(129, Serial2);

// ==========================
int  applyExpoCurve(int input, float curve, int limit = speedLimit);
int  taperToZero(int value);

void setupManualMode() {
  if (DEBUG_MODE) {
    Serial.begin(115200);
    Serial.println("=== Manual Mode Initialized ===");
  }
  setupPWMInputs();
  Serial2.begin(9600);
  delay(100);
  Serial2.write(0xAA);
  delay(10);
}

void loopManualMode() {
  static unsigned long lastFrameMicros = 0;
  const unsigned long frameIntervalMicros = 5000;

  unsigned long nowMicros = micros();
  if (nowMicros - lastFrameMicros < frameIntervalMicros) return;
  lastFrameMicros = nowMicros;

  unsigned long now = millis();

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

  if (abs(mappedDrive) <= deadZone) mappedDrive = 0;
  if (abs(mappedTurn)  <= deadZone) mappedTurn  = 0;
  if (abs(mappedDrive) > 40) mappedTurn = constrain(mappedTurn, -100, 100);
  if (mappedDrive == 0 && mappedTurn != 0) lastTurn = constrain(lastTurn, -40, 40);

  int rawCurvedDome = applyExpoCurve(domeInput, expoCurve, domeSpeedLimit);
  int curvedDome = domeFlickActive ? rawCurvedDome : rawCurvedDome * fineControlMultiplier;
  int curvedDrive = applyExpoCurve(mappedDrive, expoCurve);
  int curvedTurn  = applyExpoCurve(mappedTurn,  expoCurve);

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

  if (now - lastDriveCommandTime > motorTimeoutMs) lastDrive = 0;
  if (now - lastTurnCommandTime  > motorTimeoutMs) lastTurn  = 0;

  ST.drive(lastDrive);
  ST.turn(lastTurn);

  if (currentDomeSpeed != lastSentDomeSpeed) {
    domeMotor.motor(currentDomeSpeed);
    previousDomeMillis = now;
    lastSentDomeSpeed = currentDomeSpeed;
  }

  if (DEBUG_MODE) {
    Serial.print("DriveRaw: "); Serial.print(mappedDrive);
    Serial.print(" | DriveOut: "); Serial.print(lastDrive);
    Serial.print(" || TurnRaw: "); Serial.print(mappedTurn);
    Serial.print(" | TurnOut: "); Serial.print(lastTurn);
    Serial.print(" || DomeRaw: "); Serial.print(domeInput);
    Serial.print(" | DomeOut: "); Serial.println(currentDomeSpeed);
  }
}

static int applyExpoCurve(int input, float curve, int limit) {
  float normalized = abs(input) / 127.0;
  float curved = pow(normalized, curve) * limit;
  return (input >= 0) ? (int)curved : -(int)curved;
}

static int taperToZero(int value) {
  int taperRate = map(abs(value), 0, speedLimit, 5, taperFallRate);
  if (value > 0) value -= taperRate;
  else if (value < 0) value += taperRate;
  return constrain(value, -speedLimit, speedLimit);
}