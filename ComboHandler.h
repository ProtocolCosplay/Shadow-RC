/*
  ╔════════════════════════════════════════════════════════════╗
  ║                 ComboHandler.h - Shadow-RC                 ║
  ║────────────────────────────────────────────────────────────║
  ║ Header for the Combo Input System.                         ║
  ║ Provides `setupComboHandler()`, `updateComboHandler()`,    ║
  ║ `currentCombo`, `currentMode`, and kill switch tools.      ║
  ║                                                            ║
  ║ DO NOT MODIFY UNLESS you fully understand how mode and     ║
  ║ combo tracking is implemented across modes.                ║
  ╚════════════════════════════════════════════════════════════╝
*/

#ifndef COMBO_HANDLER_H
#define COMBO_HANDLER_H

#include <Arduino.h>

// ---------- Combo State ----------
extern int currentCombo;
extern int currentMode;
extern int lastMode;

// ---------- Setup & Loop ----------
void setupComboHandler();
void updateComboHandler();

// ---------- Helper Functions ----------
void detectToggleCombo(int pin, int &lastState, int baseCombo,
                       bool down, bool up, bool left, bool right);

void detectMomentaryCombo(int pin,
                          bool &trigDown, bool &trigUp, bool &trigLeft, bool &trigRight,
                          int baseCombo,
                          bool down, bool up, bool left, bool right);

// ---------- Kill Switch Check ----------
bool isComboModeActive();                // Legacy version
bool isComboModeActive(int mode);       // Mode-aware version

#endif
