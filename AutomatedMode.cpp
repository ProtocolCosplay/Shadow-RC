/*
  ╔═══════════════════════════════════════════════════════════════════╗
  ║               R2-D2 Automated Mode - Shadow-RC System            ║
  ║───────────────────────────────────────────────────────────────────║
  ║ Automated Mode gives R2-D2 lifelike behavior without requiring   ║
  ║ active control. Dome movements and sound playback are handled    ║
  ║ entirely through random timing, angles, and categories.          ║
  ║ Perfect for display or idle behavior during events.              ║
  ║───────────────────────────────────────────────────────────────────║

  FEATURES:
  ────────────────────────────────────────────────────────────────────
  - Randomized dome motion with variable speed, angle, and delay
  - Category-based MP3 playback from Happy, Sad, and Talking banks
  - Movement returns to center after 1–2 swings for realism
  - Gear ratio support for dome movement calibration
  - Kill switch (Combo Mode 2) immediately halts motion and audio

  TUNABLE PARAMETERS:
  ────────────────────────────────────────────────────────────────────

  -- DOME MOTION --
  - `domeMinAngle` / `domeMaxAngle`:
      Controls the random angle of dome movement per swing (in degrees).
      Higher values = longer dome sweeps. Keep under mechanical limits.

  - `maxDomeAngle`:
      Safety cap for how far dome can rotate from center in total.
      Prevents buildup of drift over multiple swings.

  - `domeMinSpeed` / `domeMaxSpeed`:
      Sets the PWM speed range (% of max power) for dome movement.
      Dome speed is randomly selected within this range per move.

  - `minMoveIntervalSec` / `maxMoveIntervalSec`:
      Sets the time window (in seconds) between each dome movement.
      R2 pauses for a random duration before executing the next action.

  - `GEAR_RATIO`:
      Converts dome angle in degrees to motor rotation for gear-driven domes.
      Adjust this if your dome gearing is different from the default.

  -- MP3 PLAYBACK --
  - `mp3MinIntervalSec` / `mp3MaxIntervalSec`:
      Controls how often MP3s are played (randomly timed).
      Larger ranges = less frequent sounds; smaller = more active.

  - `AUTO_HAPPY_START` → `AUTO_TALK_END`:
      File number ranges used for random playback in each category.
      MP3s are selected randomly from the three banks.

  - `isMP3Suppressed()`:
      Prevents MP3s from playing when MarcDuino mode is active.
      Ensures no audio conflict with scripted or pre-programmed sequences.

  DEBUGGING:
  ────────────────────────────────────────────────────────────────────
  Serial Monitor will output:
    - Dome angle, speed, and direction for each movement
    - Dome "Return to Center" notices
    - MP3 track, category, and suppression messages

  FILE LOCATION:
  ────────────────────────────────────────────────────────────────────
  This file: `AutomatedMode.cpp`
  Header:    `AutomatedMode.h`

  May the Force be with you, Builder.
  ╚═══════════════════════════════════════════════════════════════════╝
*/

#include "AutomatedMode.h"
#include <Arduino.h>
#include "MP3Handler.h"
#include "ComboHandler.h"
#include <Sabertooth.h>

// ==========================================
// === FORWARD DECLARATION ==================
// ==========================================
static void runDomeAutomation();
static void runAutoMP3(); 
static Sabertooth domeMotor(129, Serial2);  // SyRen at address 129

// ==========================================
// === DOME GEARING CONFIGURATION ===========
// ==========================================
static const float GEAR_RATIO = 360.0 / 90.0;
static const float gearRatio = 18.9;  // For duration calc

// ==========================================
// === TUNABLE PARAMETERS ===================
// ==========================================

static const int domeMinAngle = 10;         // Minimum random dome move angle (degrees)
static const int domeMaxAngle = 45;         // Maximum random dome move angle (degrees)

static const int domeMinSpeed = 20;         // Minimum dome motor speed (%) — affects movement duration
static const int domeMaxSpeed = 50;         // Maximum dome motor speed (%) — faster = quicker moves

static const unsigned long minDelayMs = 8000;   // Minimum time between dome moves (milliseconds)
static const unsigned long maxDelayMs = 12000;  // Maximum time between dome moves (milliseconds)

static const int movesBeforeCenter = 2;     // Number of random moves before returning dome to center


// --- MP3 Trigger Timing (in seconds) ---
static float mp3MinIntervalSec = 20.0;      // Minimum delay between random MP3 triggers
static float mp3MaxIntervalSec = 60.0;      // Maximum delay between random MP3 triggers

// --- MP3 Sound Bank Ranges ---
#define AUTO_HAPPY_START   001
#define AUTO_HAPPY_END     016
#define AUTO_SAD_START     031
#define AUTO_SAD_END       035
#define AUTO_TALK_START    061
#define AUTO_TALK_END      076

// ==========================================
// === INTERNAL STATE TRACKERS ==============
// ==========================================
static unsigned long lastMoveTime = 0;
static unsigned long nextMoveDelay = 0;

static unsigned long lastMP3Time = 0;
static unsigned long nextMP3Delay = 0;

static bool lastKillState = false;

static unsigned long modeEntryTime = 0;
const unsigned long modeDelayMillis = 3000;  // Wait after entering mode

// === Non-blocking dome motion state variables ===
static bool domeMoving = false;
static int domeOffset = 0;
static int moveCount = 0;
static int currentDomeSpeed = 0;
static int domeDirection = 0;
static unsigned long domeEndTime = 0;

// ==========================================
// === SETUP FUNCTION =======================
// ==========================================
void setupAutomatedMode() {
  Serial.println("=== Automated Mode Initialized ===");
  modeEntryTime = millis();
}

// ==========================================
// === MAIN LOOP FUNCTION ===================
// ==========================================
void loopAutomatedMode() {
  bool killActive = isComboModeActive(2);  // Combo 2 = Kill for Auto Mode

  if (killActive != lastKillState) {
    if (killActive) {
      Serial.println("[KILL SWITCH ACTIVE] Automation + MP3s disabled.");
    } else {
      Serial.println("[KILL SWITCH RELEASED] Automation + MP3s re-enabled.");
    }
    lastKillState = killActive;
  }

  // While kill switch is active, automation pauses but dome finishes moves
  if (!killActive) {
    runDomeAutomation();
    runAutoMP3();
  }

  delay(20);
}


// ==========================================
// === DOME AUTOMATION LOGIC ================
// ==========================================
void runDomeAutomation() {
  unsigned long now = millis();

  // === Timing calibration ===
  const float baseMsPerDegree = 1700.0 / 90.0;  // 90° at speed 30 = 1700ms
  const float curveFactor = 1.4;

  // === Speed settings ===
  const int minSpeed = 25;
  const int maxSpeed = 32;
  const int baseSpeed = 30;

  static int sequenceSpeed = baseSpeed;
  static bool sequenceStarted = false;
  static int lastMoveDirection = 0;

  // === Motion state ===
  if (domeMoving) {
    if (now >= domeEndTime) {
      domeMotor.motor(1, 0);
      domeMoving = false;
      Serial.println("[DOME] Move complete.");
    }
    return;
  }

  if (now - lastMoveTime < nextMoveDelay || now - modeEntryTime < modeDelayMillis) return;

  lastMoveTime = now;
  nextMoveDelay = random(minDelayMs, maxDelayMs + 1);

  int direction = 0;
  float angle = 0;
  bool isReturnMove = false;

  // === Lock a speed for this sequence ===
  if (!sequenceStarted) {
    sequenceSpeed = random(minSpeed, maxSpeed + 1);
    sequenceStarted = true;
    Serial.print("=== New Dome Sequence @ Speed: ");
    Serial.println(sequenceSpeed);
  }

  // === Decide movement: return to center or make a new move
  if (moveCount >= movesBeforeCenter || fabs(domeOffset) > 0.5) {
    float offsetToCorrect = domeOffset;
    direction = (offsetToCorrect >= 0) ? -1 : 1;
    angle = fabs(offsetToCorrect);
    moveCount = 0;
    sequenceStarted = false;
    isReturnMove = true;

    Serial.print("[DOME] Returning to center:  ");
  } else {
    direction = (random(0, 2) == 0) ? -1 : 1;
    angle = random(domeMinAngle, domeMaxAngle + 1);
    moveCount++;

    Serial.print("[DOME] Move ");
    Serial.print(moveCount);
    Serial.print(":  ");
    Serial.println(direction > 0 ? "RIGHT" : "LEFT");
  }

  // === Timing scaling based on speed
  float speedRatio = baseSpeed / (float)sequenceSpeed;
  float scaleFactor = pow(speedRatio, curveFactor);
  float adjustedMsPerDegree = baseMsPerDegree * scaleFactor;

  // === Direction-specific correction
  if (!isReturnMove && direction > 0) {
    adjustedMsPerDegree *= 1.06;  // Right turns: slightly slower (reduce overshoot)
  } else if (!isReturnMove && direction < 0) {
    adjustedMsPerDegree *= 0.96;  // Left turns: slightly faster (prevent undershoot)
  }

  // === Final calculated duration
  unsigned long duration = (unsigned long)(angle * adjustedMsPerDegree);

  // === Offset tracking (apply to ALL moves)
  float actualAngleMoved = (float)duration / adjustedMsPerDegree;
  domeOffset += direction * actualAngleMoved;

  // === Debug output
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

  // === Execute move
  domeMotor.motor(1, direction * sequenceSpeed);
  domeEndTime = now + duration;
  domeDirection = direction;
  currentDomeSpeed = sequenceSpeed;
  domeMoving = true;

  if (!isReturnMove) {
    lastMoveDirection = direction;
  }
}




// ==========================================
// === MP3 TRIGGER LOGIC ====================
// ==========================================
void runAutoMP3() {
  unsigned long now = millis();

  if (now - lastMP3Time > nextMP3Delay) {
    lastMP3Time = now;

    if (!isMP3Suppressed()) {
      int category = random(0, 3);  // 0 = Happy, 1 = Sad, 2 = Talking
      int track = 0;
      const char* label = "";

      if (category == 0) {
        track = random(AUTO_HAPPY_START, AUTO_HAPPY_END + 1);
        label = "Happy";
      } else if (category == 1) {
        track = random(AUTO_SAD_START, AUTO_SAD_END + 1);
        label = "Sad";
      } else {
        track = random(AUTO_TALK_START, AUTO_TALK_END + 1);
        label = "Talking";
      }

      Serial.print("[MP3] Random ");
      Serial.print(label);
      Serial.print(" → Track ");
      Serial.println(track);

       // Uncomment if using onboard MP3 player:
       mp3.trigger(track);
    } else {
      Serial.println("[MP3] Suppressed – skipping random MP3 playback.");
    }

    nextMP3Delay = random(mp3MinIntervalSec * 1000, mp3MaxIntervalSec * 1000);
  }
}
