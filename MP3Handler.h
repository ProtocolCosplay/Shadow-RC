/*
  ╔════════════════════════════════════════════════════════════╗
  ║                  MP3Handler.h - Shadow-RC                  ║
  ║────────────────────────────────────────────────────────────║
  ║ Header for MP3 Trigger System.                             ║
  ║ Declares `setupMP3Handler()` and `updateMP3Handler()`      ║
  ║ for button-based sound playback using the MP3 board.       ║
  ║                                                            ║
  ║ DO NOT EDIT unless you're adding new sound categories or   ║
  ║ changing trigger logic.                                    ║
  ╚════════════════════════════════════════════════════════════╝
*/

#ifndef MP3_HANDLER_H
#define MP3_HANDLER_H

#include <Arduino.h>
#include <MP3Trigger.h>  // ✅ Added to support the V2.4 SparkFun MP3 Trigger

// === Setup and Main Loop ===
void setupMP3Handler();
void updateMP3Handler();

// === MP3 File Tracker ===
extern int currentMP3;
extern MP3Trigger mp3;  // ✅ Declare the mp3 object used in .cpp

// === Suppression Toggle System ===
void suppressMP3Handler(unsigned long duration = 30000, int combo = -1, const char* label = nullptr);
void clearMP3Suppression();
bool isMP3Suppressed();

#endif