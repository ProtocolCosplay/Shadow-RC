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
#include <Sabertooth.h>

// === FUNCTION DECLARATIONS ===
static void updateEncoder();

// ==========================
//     Motor + Encoder Setup
// ==========================
static Sabertooth domeMotor(129, Serial2);
const int encoderPinA = 19;
const int encoderPinB = 20;

volatile long encoderTicks = 0;
static bool lastA = 0;
static bool lastB = 0;

unsigned long motorStartTime = 0;
unsigned long lastMotorSend = 0;
bool motorRunning = true;

// ==========================
//        Setup
// ==========================
void setupAutomatedMode() {
  Serial.begin(115200);
  Serial.println("🔥🔥 IF YOU SEE THIS, YOU ARE RUNNING THE RIGHT VERSION 🔥🔥");

  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);
  lastA = digitalRead(encoderPinA);
  lastB = digitalRead(encoderPinB);
  attachInterrupt(digitalPinToInterrupt(encoderPinA), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderPinB), updateEncoder, CHANGE);

  encoderTicks = 0;

  Serial2.begin(9600);
  delay(1000);
  Serial2.write(0xAA);
  delay(100);

  motorStartTime = millis();
  motorRunning = true;
}

// ==========================
//         Loop
// ==========================
void loopAutomatedMode() {
  unsigned long now = millis();

  if (motorRunning) {
    if (now - motorStartTime < 2000) {
      if (now - lastMotorSend > 50) {
        domeMotor.motor(30);
        lastMotorSend = now;
      }
    } else {
      domeMotor.motor(0);
      motorRunning = false;
      Serial.println("Motor OFF");
    }
  }

  static unsigned long lastPrint = 0;
  if (now - lastPrint > 250) {
    lastPrint = now;
    Serial.print("Ticks: ");
    Serial.println(encoderTicks);
  }
}

// ==========================
//        ISR
// ==========================
void updateEncoder() {
  bool currentA = digitalRead(encoderPinA);
  bool currentB = digitalRead(encoderPinB);

  if (lastA != currentA || lastB != currentB) {
    if (lastA == currentB) encoderTicks++;
    else encoderTicks--;
  }

  lastA = currentA;
  lastB = currentB;
}
