/*
  ╔════════════════════════════════════════════════════════════╗
  ║              PWMInputHandler.h - Shadow-RC                 ║
  ║────────────────────────────────────────────────────────────║
  ║ Header for centralized PWM reading from RC receivers.      ║
  ║ Provides `getPWMValue_CHxY()` functions for each channel.  ║
  ║                                                            ║
  ║ DO NOT EDIT unless you are changing receiver hardware OR   ║
  ║ you fully understand interrupt-based PWM handling.         ║
  ╚════════════════════════════════════════════════════════════╝
*/

#ifndef PWMINPUTHANDLER_H
#define PWMINPUTHANDLER_H

// Define PWM signal tracking variables
extern volatile int ch1_value;   // Turn (from Controller A)
extern volatile int ch2_value;   // Drive (from Controller A)
extern volatile int ch1b_value;  // Dome (from Controller B)

// Define pin assignments for PWM signal inputs (update these according to your actual wiring)
#define CH1_PIN    2  // Example pin for Channel 1 (Controller A)
#define CH2_PIN    3  // Example pin for Channel 2 (Controller A)
#define CH1B_PIN   18 // Example pin for Channel 1 (Controller B)

// Function declarations
void setupPWMInputs();
int getPWMValue_CH1A();
int getPWMValue_CH2A();
int getPWMValue_CH1B();

// Interrupt handler function declarations
void ch1_rise();
void ch1_fall();
void ch2_rise();
void ch2_fall();
void ch1b_rise();
void ch1b_fall();

#endif
