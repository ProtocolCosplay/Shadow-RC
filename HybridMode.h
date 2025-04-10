/*
  ╔════════════════════════════════════════════════════════════╗
  ║                   HybridMode.h - Shadow-RC                 ║
  ║────────────────────────────────────────────────────────────║
  ║ Header for Hybrid Mode (manual drive + auto dome + MP3).   ║
  ║ Declares `setupHybridMode()` and `loopHybridMode()`        ║
  ║ for hybrid behavior logic.                                 ║
  ║                                                            ║
  ║ DO NOT EDIT unless you're modifying Hybrid Mode behavior   ║
  ║ and fully understand the automation structure.             ║
  ╚════════════════════════════════════════════════════════════╝
*/

#ifndef HYBRIDMODE_H
#define HYBRIDMODE_H

#include <Arduino.h>
#include "PWMInputHandler.h"

extern int domeMinAngle;
extern int domeMaxAngle;
extern int domeCurrentAngle;

void setupHybridMode();
void loopHybridMode();

#endif
