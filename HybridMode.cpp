/*
  ╔═══════════════════════════════════════════════════════════════════╗
  ║                R2-D2 Hybrid Mode - Shadow-RC System              ║
  ║───────────────────────────────────────────────────────────────────║
  ║ Hybrid Mode blends manual drive control with automated dome      ║
  ║ motion and randomized sound playback. Ideal for walk-arounds,    ║
  ║ it lets the operator steer while the dome performs lifelike      ║
  ║ movements and R2 vocalizes independently.                        ║
  ║───────────────────────────────────────────────────────────────────║

  FEATURES:
  ────────────────────────────────────────────────────────────────────
  - Manual drive control using Controller A:
       - CH2A = Forward/back
       - CH1A = Turn left/right
  - Dome automation with randomized angles, timing, and speed
  - Optional random MP3 playback using sound banks
  - Kill switch (Combo Mode 3) disables all automation + sounds
  - Expo curve for natural joystick feel and smoother steering

  TUNABLE PARAMETERS:
  ────────────────────────────────────────────────────────────────────

  -- DRIVE CONTROL --
  - `expoCurve`:
      Affects how sensitive joystick input is near the center.
      Higher values (2.5–4.0) make small stick movements more gentle.
      Lower values (1.0–2.0) make controls more twitchy and responsive.

  - `speedLimit`:
      Sets the maximum speed (0–127) sent to the Sabertooth drive system.
      Useful for capping top speed to match your droid’s gearing or indoor space.

  - `deadZone`:
      Ignores joystick input within this threshold (e.g. ±1).
      Helps eliminate jitter when sticks are near the center but not perfectly still.

  - `taperFallRate`:
      Controls how quickly turning stops when the stick is released.
      Higher = more aggressive snap-back to zero; lower = smoother glide to a stop.

  - `motorTimeoutMs`:
      If no input is received within this time, drive and turn output is cut off.
      Acts as a safety feature in case of signal loss (e.g. 150ms = 0.15 sec).

  -- DOME AUTOMATION --
  - `minMoveIntervalSec` / `maxMoveIntervalSec`:
      Dome will wait a random time between these values before each move.
      Larger values = slower, more subtle behavior; shorter = livelier.

  - `domeMinAngleDeg` / `domeMaxAngleDeg`:
      The dome will pick a random rotation angle within this range.
      Movement alternates left/right and returns to center when done.

  - `domeMinSpeedPercent` / `domeMaxSpeedPercent`:
      Sets the PWM speed range for dome motion (0–100%).
      Higher speeds create sharper, snappier dome moves.

  -- MP3 PLAYBACK (OPTIONAL) --
  - `DISABLE_MP3`:
      Defined by default to turn off MP3 playback during testing or setups without sound.
      Remove or comment out this line to enable MP3 support.

  - `HYBRID_HAPPY_START`, etc.:
      Define the file number ranges used for random sounds.
      Separate categories (Happy, Sad, Talking) are picked randomly.

  - `isMP3Suppressed()`:
      Prevents random sound triggers when MarcDuino is active (e.g. Full Awake Mode).
      Ensures no interference between manual and scripted sequences.

  DEBUGGING:
  ────────────────────────────────────────────────────────────────────
  Real-time debug output prints the following to Serial Monitor:
     - Drive and turn joystick input and output values
     - Dome PWM signal
     - MP3 triggers (category and track) if enabled

  FILE LOCATION:
  ────────────────────────────────────────────────────────────────────
  This file: `HybridMode.cpp`
  Header:    `HybridMode.h`

  May the Force be with you, Builder.
  ╚═══════════════════════════════════════════════════════════════════╝
*/

#include "HybridMode.h"
#include "MP3Handler.h"
#include "ComboHandler.h"
#include "PWMInputHandler.h"
#include <Arduino.h>
#include <Sabertooth.h>

// #define DISABLE_MP3  // ✅ Leave this line commented out to ENABLE MP3s

#ifndef DISABLE_MP3
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Forward Declarations
// ─────────────────────────────────────────────────────────────────────────────
void automationMode();
static int applyExpoCurve(int input, float curve);
static int taperToZero(int value);
void runDomeAutomation();    
void runAutoMP3();           
void playMP3Track(int track);         

// ─────────────────────────────────────────────────────────────────────────────
// Sabertooth Motor Controller Setup
// ─────────────────────────────────────────────────────────────────────────────
static Sabertooth ST(128, Serial2);  // Drive motor controller (address 128)
static Sabertooth domeMotor(129, Serial2);  // SyRen at address 129

// ─────────────────────────────────────────────────────────────────────────────
// TUNABLE PARAMETERS — DRIVE & INPUT
// ─────────────────────────────────────────────────────────────────────────────
static float expoCurve = 1;                     // Shapes input curve (1 = linear, higher = smoother)
static int speedLimit = 25;                     // Max drive/turn speed (0–127)
static int deadZone = 0;                        // Joystick input below this is ignored
static int taperFallRate = 60;                  // Turn deceleration rate (higher = faster snap)
static int fineControlMultiplier = 2;           // Boosts dome speed during flicks
static const unsigned long motorTimeoutMs = 150; // Time in ms before motors stop if no input

// ─────────────────────────────────────────────────────────────────────────────
// TUNABLE PARAMETERS — AUTOMATED DOME
// ─────────────────────────────────────────────────────────────────────────────
static float minMoveIntervalSec = 10.0;          // Minimum time between automated dome moves
static float maxMoveIntervalSec = 30.0;          // Maximum time between automated dome moves
static int domeMinAngleDeg = 10;                 // Minimum angle to turn dome (degrees)
static int domeMaxAngleDeg = 90;                 // Maximum angle to turn dome (degrees)
static int domeMinSpeedPercent = 10;             // Minimum speed for dome moves (percent)
static int domeMaxSpeedPercent = 50;             // Maximum speed for dome moves (percent)


// === Dome Gear Ratio ===
// This converts dome angle degrees into total motor rotation angle
// Needed to generate realistic dome motor movement
static const float GEAR_RATIO = 360.416 / 50.7;

// ─────────────────────────────────────────────────────────────────────────────
// TUNABLE PARAMETERS — MP3 BANKS
// ─────────────────────────────────────────────────────────────────────────────
#define HYBRID_HAPPY_START   001
#define HYBRID_HAPPY_END     016
#define HYBRID_SAD_START     031
#define HYBRID_SAD_END       035
#define HYBRID_TALK_START    061
#define HYBRID_TALK_END      076

#ifndef DISABLE_MP3
static unsigned long lastMP3Time = 0;
static unsigned long nextMP3Delay = 0; 
#endif

// ─────────────────────────────────────────────────────────────────────────────
// INTERNAL STATE
// ─────────────────────────────────────────────────────────────────────────────
static int domeInput = 0;
static int currentDomeSpeed = 0;
static int lastSentDomeSpeed = 0;
static unsigned long previousDomeMillis = 0;

static int lastDrive = 0;
static int lastTurn = 0;
static int savedTurnSpeed = 0;
static unsigned long lastDriveCommandTime = 0;
static unsigned long lastTurnCommandTime = 0;

static bool wasTurnInputActive = false;
static bool lastKillState = false;

static unsigned long modeEntryTime = 0;
static const unsigned long modeDelayMillis = 3000;

// ───── Dome Automation State ─────
enum DomeState { IDLE, MOVE_1, MOVE_2, RETURN_TO_CENTER };
static DomeState domeState = IDLE;
static unsigned long domeTimer = 0;
static unsigned long domeDelay = 0;
static int domeMovesToMake = 0;
static int domeMoveCount = 0;
static float domeAngleTracker = 0;

// ─────────────────────────────────────────────────────────────────────────────
// SETUP FUNCTION
// ─────────────────────────────────────────────────────────────────────────────
void setupHybridMode() {
  setupPWMInputs();
  modeEntryTime = millis();

  Serial2.begin(9600);
  delay(100);
  Serial2.write(0xAA);  // Sync byte for Sabertooth/SyRen
  delay(10);
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN LOOP
// ─────────────────────────────────────────────────────────────────────────────
void loopHybridMode() {
  unsigned long now = millis();

  int rawTurn  = ch1_value;
  int rawDrive = ch2_value;
  int rawDome  = ch1b_value;

  int mappedTurn  = map(constrain(rawTurn,  1000, 2000), 1000, 2000, -127, 127);
  int mappedDrive = map(constrain(rawDrive, 1000, 2000), 1000, 2000, -127, 127);

  bool driveInDeadzone = (abs(mappedDrive) <= deadZone);
  bool turnInDeadzone  = (abs(mappedTurn) <= deadZone);
  if (driveInDeadzone) mappedDrive = 0;
  if (turnInDeadzone) mappedTurn = 0;

  if (abs(mappedDrive) > 80) mappedTurn = constrain(mappedTurn, -40, 40);

  int curvedDrive = applyExpoCurve(mappedDrive, expoCurve);
  int curvedTurn  = applyExpoCurve(mappedTurn,  expoCurve);

  lastDrive = curvedDrive;
  lastDriveCommandTime = now;

  if (mappedTurn == 0 && wasTurnInputActive) {
    lastTurn = taperToZero(lastTurn != 0 ? lastTurn : savedTurnSpeed);
  } else if (mappedTurn == 0) {
    lastTurn = 0;
  } else {
    savedTurnSpeed = curvedTurn;
    lastTurn = curvedTurn;
    lastTurnCommandTime = now;
  }

  wasTurnInputActive = (mappedTurn != 0);

  bool killActive = isComboModeActive(3);
  if (killActive != lastKillState) {
    Serial.println(killActive ? "[KILL SWITCH ACTIVE] Automation + MP3s disabled."
                              : "[KILL SWITCH RELEASED] Automation + MP3s re-enabled.");
    lastKillState = killActive;
  }

  if (killActive) {
    lastDrive = 0;
    lastTurn = 0;
  }

  ST.drive(lastDrive);
  ST.turn(lastTurn);

  if (!killActive) {
  runDomeAutomation();
  runAutoMP3();
  } else {
    analogWrite(46, 0);
  }

  if (currentDomeSpeed != lastSentDomeSpeed || now - previousDomeMillis >= 50) {
    analogWrite(46, currentDomeSpeed);
    previousDomeMillis = now;
    lastSentDomeSpeed = currentDomeSpeed;
  }

  if (now - lastDriveCommandTime > motorTimeoutMs) lastDrive = 0;
  if (now - lastTurnCommandTime > motorTimeoutMs) lastTurn = 0;

  Serial.print("DriveRaw: "); Serial.print(mappedDrive);
  Serial.print(" | DriveOut: "); Serial.print(lastDrive);
  Serial.print(" || TurnRaw: "); Serial.print(mappedTurn);
  Serial.print(" | TurnOut: "); Serial.print(lastTurn);
  Serial.print(" || DomeRaw: "); Serial.print(domeInput);
  Serial.print(" | DomeOut: "); Serial.println(currentDomeSpeed);

  delay(20);
}

// ─────────────────────────────────────────────────────────────
// Split Automation System — Dome + MP3 (non-blocking)
// ─────────────────────────────────────────────────────────────

void runDomeAutomation() {
  static unsigned long lastMoveTime = 0;
  static unsigned long nextMoveDelay = 0;
  static bool domeMoving = false;
  static int domeOffset = 0;
  static int moveCount = 0;
  static int domeDirection = 0;
  static int sequenceSpeed = 30;
  static bool sequenceStarted = false;
  static unsigned long domeEndTime = 0;

  unsigned long now = millis();
  if (now - modeEntryTime < 3000) return;

  // === Timing setup
  const float baseMsPerDegree = 1700.0 / 90.0;
  const float curveFactor = 1.4;
  const int baseSpeed = 30;
  const int minSpeed = 25;
  const int maxSpeed = 32;

  if (domeMoving) {
    if (now >= domeEndTime) {
      domeMotor.motor(1, 0);
      domeMoving = false;
      Serial.println("[DOME] Move complete.");
    }
    return;
  }

  if (now - lastMoveTime < nextMoveDelay) return;
  lastMoveTime = now;
  nextMoveDelay = random(minMoveIntervalSec * 1000, maxMoveIntervalSec * 1000);

  if (!sequenceStarted) {
    sequenceSpeed = random(minSpeed, maxSpeed + 1);
    sequenceStarted = true;
    Serial.print("=== New Dome Sequence @ Speed: ");
    Serial.println(sequenceSpeed);
  }

  int direction = 0;
  float angle = 0;
  bool isReturnMove = false;

  if (moveCount >= 2 || fabs(domeOffset) > 0.5) {
    direction = (domeOffset >= 0) ? -1 : 1;
    angle = fabs(domeOffset);
    moveCount = 0;
    sequenceStarted = false;
    isReturnMove = true;
    Serial.print("[DOME] Returning to center:  ");
  } else {
    direction = random(0, 2) == 0 ? -1 : 1;
    angle = random(domeMinAngleDeg, domeMaxAngleDeg + 1);
    moveCount++;
    Serial.print("[DOME] Move ");
    Serial.print(moveCount);
    Serial.print(":  ");
    Serial.println(direction > 0 ? "RIGHT" : "LEFT");
  }

  float speedRatio = baseSpeed / (float)sequenceSpeed;
  float scaleFactor = pow(speedRatio, curveFactor);
  float adjustedMsPerDegree = baseMsPerDegree * scaleFactor;

  if (!isReturnMove && direction > 0) adjustedMsPerDegree *= 1.06;
  if (!isReturnMove && direction < 0) adjustedMsPerDegree *= 0.96;

  unsigned long duration = (unsigned long)(angle * adjustedMsPerDegree);
  float actualAngleMoved = (float)duration / adjustedMsPerDegree;
  domeOffset += direction * actualAngleMoved;

  Serial.print("Angle: ");
  Serial.print(angle);
  Serial.print("°   Actual: ");
  Serial.print(actualAngleMoved);
  Serial.print("°   Speed: ");
  Serial.print(sequenceSpeed);
  Serial.print("   Duration: ");
  Serial.print(duration);
  Serial.print(" ms   Offset: ");
  Serial.println(domeOffset);

  domeMotor.motor(1, direction * sequenceSpeed);
  domeEndTime = now + duration;
  domeDirection = direction;
  currentDomeSpeed = sequenceSpeed;
  domeMoving = true;
}


void runAutoMP3() {
  static unsigned long lastMP3Time = 0;
  static unsigned long nextMP3Delay = 0;

  unsigned long now = millis();

  if (now - modeEntryTime < 3000) return;

  if (now - lastMP3Time > nextMP3Delay && !isMP3Suppressed()) {
    lastMP3Time = now;

    int category = random(0, 3);
    int track = 0;
    const char* label = "";

    if (category == 0) {
      track = random(HYBRID_HAPPY_START, HYBRID_HAPPY_END + 1);
      label = "Happy";
    } else if (category == 1) {
      track = random(HYBRID_SAD_START, HYBRID_SAD_END + 1);
      label = "Sad";
    } else {
      track = random(HYBRID_TALK_START, HYBRID_TALK_END + 1);
      label = "Talking";
    }

    Serial.print("[MP3] Random ");
    Serial.print(label);
    Serial.print(" → Track ");
    Serial.println(track);

    playMP3Track(track);
    nextMP3Delay = random(5000, 15000);
  }
}



// ─────────────────────────────────────────────────────────────────────────────
// Input Curve + Taper Helpers
// ─────────────────────────────────────────────────────────────────────────────
static int applyExpoCurve(int input, float curve) {
  float normalized = abs(input) / 127.0;
  float curved = pow(normalized, curve) * speedLimit;
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
