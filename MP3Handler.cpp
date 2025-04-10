/*
  ╔════════════════════════════════════════════════════════════════════╗
  ║                     MP3Handler.cpp - Shadow-RC System              ║
  ║────────────────────────────────────────────────────────────────────║
  ║ Handles all MP3 sound trigger logic using PWM input from both RC   ║
  ║ controllers. When the dome joysticks are paired with button presses║
  ║ (CH3–CH6), this file selects and plays randomized MP3 tracks over  ║
  ║ Serial1 to the SparkFun MP3 Trigger.                               ║
  ║                                                                    ║
  ║ Supports automatic MP3 suppression when MarcDuino is active, and   ║
  ║ includes dedicated sound banks per button and controller group.    ║
  ║────────────────────────────────────────────────────────────────────║

  FEATURES:
  ─────────────────────────────────────────────────────────────────────
  - Supports 8 MP3 categories:
      • Happy, Sad, Talking, Yelling
      • Classic, Dance, Singing, Lines
  - PWM input edge detection (toggle-based and momentary support)
  - Randomized file selection from each category’s track range
  - Automatic debounce and suppression during combo input or MarcDuino use
  - Serial1 (TX1 / pin 18) output to MP3 Trigger (SparkFun-compatible)

  SOUND BANK ORGANIZATION:
  ─────────────────────────────────────────────────────────────────────
  MP3 files on the SD card **must** follow strict naming conventions:
    - Files must be named: `001.mp3`, `002.mp3`, ..., `999.mp3`
    - Each sound category uses a numeric range:

      • Happy     → 001–016
      • Sad       → 031–035
      • Talking   → 061–076
      • Yelling   → 091–103
      • Classic   → 121–123
      • Dance     → 151–156
      • Singing   → 181–186
      • Lines     → 211–212

  - **You may add more files** within each bank as long as:
      • The file numbers remain within the defined range
      • They follow the correct 3-digit naming format (e.g., `061.mp3`)
      • The MP3Trigger can read the filenames in the root directory

  - Each bank has **up to 15 free slots** for expansion, depending on usage.

  MARCDUINO COMPATIBILITY:
  ─────────────────────────────────────────────────────────────────────
  - `disableMP3Triggers()` and `enableMP3Triggers()` provide full integration
    with MarcDuino-driven mode changes (e.g., Awake+, Quiet).
  - `isMP3Suppressed()` allows other files (like Hybrid/Auto mode) to check if
    MP3s should be skipped due to MarcDuino activity.

  PIN MAPPINGS:
  ─────────────────────────────────────────────────────────────────────
  - Controller A Buttons:
      CH3 → Happy    | CH4 → Sad
      CH5 → Talking  | CH6 → Yelling (momentary)
  - Controller B Buttons:
      CH3 → Classic  | CH4 → Dance
      CH5 → Singing  | CH6 → Lines   (momentary)

  DEBUGGING:
  ─────────────────────────────────────────────────────────────────────
  Serial monitor displays:
    - Track type and ID number on every successful trigger
    - Trigger suppression status
    - Real-time feedback during combo overrides

  ⚠️  WARNING: DO NOT EDIT UNLESS YOU KNOW WHAT YOU ARE DOING ⚠️
  ─────────────────────────────────────────────────────────────────────
  This file handles time-sensitive edge detection and sound dispatching.
  Improper changes can:
     • Prevent sounds from triggering
     • Break button timing detection
     • Cause audio glitches or lockups
     • Interfere with MarcDuino suppression logic

  Only edit this file if:
     • You are adding new sound banks OR
     • You fully understand the PWM input system + MP3Trigger protocol

  FILE LOCATION:
  ─────────────────────────────────────────────────────────────────────
  This file: `MP3Handler.cpp`  
  Header:    `MP3Handler.h`

  May the Force be with you, Builder.
  ╚════════════════════════════════════════════════════════════════════╝
*/


#include "MP3Handler.h"
#include "ComboHandler.h"
#include <Arduino.h>
#include <MP3Trigger.h>

// ──────────────────────────────────────────────────────────────────────
// SELECT YOUR MP3 BOARD HERE:
// 1 = SparkFun MP3 Trigger (V2.4)
// 2 = YX5300 Serial MP3 Module (KOOKBOOK, Diann, etc.)
// 3 = DFPlayer Mini (Serial Mode)
// ──────────────────────────────────────────────────────────────────────
#define ACTIVE_MP3_BOARD 1

// ─────────────────────────────────────────────────────────────────────────────
// PIN DEFINITIONS — RC Channels (PWM Input Pins)
// ─────────────────────────────────────────────────────────────────────────────
#define CH3_PIN_A 22
#define CH4_PIN_A 24
#define CH5_PIN_A 26
#define CH6_PIN_A 28

#define CH3_PIN_B 25
#define CH4_PIN_B 27
#define CH5_PIN_B 29
#define CH6_PIN_B 31

// ─────────────────────────────────────────────────────────────────────────────
// PWM THRESHOLDS + TIMING
// ─────────────────────────────────────────────────────────────────────────────
const int HIGH_THRESHOLD   = 1700;
const int LOW_THRESHOLD    = 1300;
const int CH6_HIGH_MIN     = 1985;
const int CH6_HIGH_MAX     = 1995;
const int DEBOUNCE_DELAY   = 50;
const int VALID_PWM_MIN    = 900;
const int VALID_PWM_MAX    = 2200;

// ─────────────────────────────────────────────────────────────────────────────
// MP3 SOUND BANK ASSIGNMENTS
// ─────────────────────────────────────────────────────────────────────────────
const int BANK_HAPPY_START    = 1;
const int BANK_HAPPY_END      = 16;
const int BANK_SAD_START      = 31;
const int BANK_SAD_END        = 35;
const int BANK_TALKING_START  = 61;
const int BANK_TALKING_END    = 76;
const int BANK_YELLING_START  = 91;
const int BANK_YELLING_END    = 103;
const int BANK_CLASSIC_START  = 121;
const int BANK_CLASSIC_END    = 123;
const int BANK_DANCE_START    = 151;
const int BANK_DANCE_END      = 156;
const int BANK_SINGING_START  = 181;
const int BANK_SINGING_END    = 186;
const int BANK_LINES_START    = 211;
const int BANK_LINES_END      = 212;

// ─────────────────────────────────────────────────────────────────────────────
// STATE VARIABLES
// ─────────────────────────────────────────────────────────────────────────────
bool mp3TriggersEnabled = true;

int lastMP3_CH3A = -1, lastMP3_CH4A = -1, lastMP3_CH5A = -1;
int lastMP3_CH3B = -1, lastMP3_CH4B = -1, lastMP3_CH5B = -1;

bool hasTriggeredMP3_CH6A = false;
bool hasTriggeredMP3_CH6B = false;

int currentMP3 = 0;

// ─────────────────────────────────────────────────────────────────────────────
// MP3 TRIGGER INSTANCE
// ─────────────────────────────────────────────────────────────────────────────
MP3Trigger mp3;

// ─────────────────────────────────────────────────────────────────────────────
// FORWARD DECLARATIONS
// ─────────────────────────────────────────────────────────────────────────────
void checkToggleAnyEdge(int pin, int &lastState, int startFile, int endFile, const char* label);
void checkMomentary(int pin, bool &hasTriggered, int startFile, int endFile, const char* label);
void playMP3Track(int track);

// ─────────────────────────────────────────────────────────────────────────────
// INITIALIZATION
// ─────────────────────────────────────────────────────────────────────────────
void setupMP3Handler() {
#if ACTIVE_MP3_BOARD == 1
  Serial1.begin(38400);     // SparkFun MP3 Trigger
  mp3.setup(&Serial1);
  delay(1000);

#else
  Serial1.begin(9600);      // YX5300 or DFPlayer
  delay(500);               
#endif

  pinMode(CH3_PIN_A, INPUT);
  pinMode(CH4_PIN_A, INPUT);
  pinMode(CH5_PIN_A, INPUT);
  pinMode(CH6_PIN_A, INPUT);

  pinMode(CH3_PIN_B, INPUT);
  pinMode(CH4_PIN_B, INPUT);
  pinMode(CH5_PIN_B, INPUT);
  pinMode(CH6_PIN_B, INPUT);
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN UPDATE LOOP
// ─────────────────────────────────────────────────────────────────────────────
void updateMP3Handler() {
  if (!mp3TriggersEnabled || isComboModeActive(currentMode)) {
    Serial.print(">> MP3Handler: Triggers disabled or combo active | ");
    Serial.print("Enabled: ");
    Serial.print(mp3TriggersEnabled ? "YES" : "NO");
    Serial.print(" | Combo Active: ");
    Serial.println(isComboModeActive(currentMode) ? "YES" : "NO");

    lastMP3_CH3A = lastMP3_CH4A = lastMP3_CH5A = -1;
    lastMP3_CH3B = lastMP3_CH4B = lastMP3_CH5B = -1;
    hasTriggeredMP3_CH6A = hasTriggeredMP3_CH6B = false;
    return;
  }

  // Channel A
  checkToggleAnyEdge(CH3_PIN_A, lastMP3_CH3A, BANK_HAPPY_START, BANK_HAPPY_END, "Happy");
  checkToggleAnyEdge(CH4_PIN_A, lastMP3_CH4A, BANK_SAD_START, BANK_SAD_END, "Sad");
  checkToggleAnyEdge(CH5_PIN_A, lastMP3_CH5A, BANK_TALKING_START, BANK_TALKING_END, "Talking");
  checkMomentary(CH6_PIN_A, hasTriggeredMP3_CH6A, BANK_YELLING_START, BANK_YELLING_END, "Yelling");

  // Channel B
  checkToggleAnyEdge(CH3_PIN_B, lastMP3_CH3B, BANK_CLASSIC_START, BANK_CLASSIC_END, "Classic");
  checkToggleAnyEdge(CH4_PIN_B, lastMP3_CH4B, BANK_DANCE_START, BANK_DANCE_END, "Dance");
  checkToggleAnyEdge(CH5_PIN_B, lastMP3_CH5B, BANK_SINGING_START, BANK_SINGING_END, "Singing");
  checkMomentary(CH6_PIN_B, hasTriggeredMP3_CH6B, BANK_LINES_START, BANK_LINES_END, "Lines");

  delay(DEBOUNCE_DELAY);
}

// ─────────────────────────────────────────────────────────────────────────────
// TOGGLE BUTTON HANDLER (CH3–CH5)
// ─────────────────────────────────────────────────────────────────────────────
void checkToggleAnyEdge(int pin, int &lastState, int startFile, int endFile, const char* label) {
  int pwm = pulseIn(pin, HIGH, 30000);
  if (pwm == 0 || pwm < VALID_PWM_MIN || pwm > VALID_PWM_MAX) return;

  int newState = (pwm > HIGH_THRESHOLD) ? HIGH : (pwm < LOW_THRESHOLD) ? LOW : lastState;

  if (lastState == -1) {
    lastState = newState;
    return;
  }

  if (newState != lastState) {
    int randomTrack = random(startFile, endFile + 1);
    currentMP3 = randomTrack;

    Serial.print(">> MP3 Trigger [");
    Serial.print(label);
    Serial.print("]: Track ");
    Serial.println(currentMP3);

    playMP3Track(currentMP3); 
    lastState = newState;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// MOMENTARY BUTTON HANDLER (CH6)
// ─────────────────────────────────────────────────────────────────────────────
void checkMomentary(int pin, bool &hasTriggered, int startFile, int endFile, const char* label) {
  static unsigned long lastTriggerTime = 0;
  int pwm = pulseIn(pin, HIGH, 30000);

  if (pwm > HIGH_THRESHOLD && !hasTriggered) {
    int randomTrack = random(startFile, endFile + 1);
    currentMP3 = randomTrack;

    Serial.print(">> MP3 Momentary Trigger [");
    Serial.print(label);
    Serial.print("]: Track ");
    Serial.println(currentMP3);

    playMP3Track(currentMP3); 
    hasTriggered = true;
    lastTriggerTime = millis();
  }

  if (hasTriggered && millis() - lastTriggerTime > 50) {
    hasTriggered = false;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// MP3 TRIGGER CONTROL
// ─────────────────────────────────────────────────────────────────────────────
void disableMP3Triggers() {
  mp3TriggersEnabled = false;
  Serial.println(">> MP3Handler: Triggers DISABLED by MarcDuino mode.");
}

void enableMP3Triggers() {
  mp3TriggersEnabled = true;
  Serial.println(">> MP3Handler: Triggers RE-ENABLED by Quiet Mode.");
}

bool isMP3Suppressed() {
  return !mp3TriggersEnabled;
}

// ─────────────────────────────────────────────────────────────────────────────
// UNIVERSAL TRACK PLAYER — Supports SparkFun, YX5300, DFPlayer
// ─────────────────────────────────────────────────────────────────────────────
void playMP3Track(int track) {
#if ACTIVE_MP3_BOARD == 1
  // SparkFun MP3 Trigger
  mp3.trigger(track);

#elif ACTIVE_MP3_BOARD == 2 || ACTIVE_MP3_BOARD == 3
  // YX5300 / DFPlayer – Play track N from root folder
  byte command[] = {
    0x7E, 0xFF, 0x06, 0x03, 0x00, 0x00, (byte)track, 0xEF
  };
  Serial1.write(command, sizeof(command));
#endif
}
