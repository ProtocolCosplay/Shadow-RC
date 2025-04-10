# Shadow-RC
### Modular R2-D2 Control System for Arduino Mega + Dual RC Controllers

Shadow-RC is a highly customizable, real-world control system for R2-D2 droids. Built on the Arduino Mega 2560 platform, it uses dual 6-channel RC controllers, Sabertooth and SyRen motor drivers (via packetized serial), and optionally supports MP3 playback and full MarcDuino integration.

The system includes four operational modes, a robust 32-slot combo trigger system, joystick flick detection, dome automation, and full parameter tuning. Shadow-RC is ideal for builders who want a reliable, analog-feeling droid control system with flexible expansion.

---

## Features

- **Manual Mode** – Responsive, direct joystick control of drive and dome
- **Carpet Mode** – Boosted drive power for navigating carpet and rough terrain
- **Hybrid Mode** – Adds dome automation and ambient sound to manual drive
- **Automated Mode** – Fully autonomous dome movement and sound (no RC input)
- **Combo System** – 32 joystick + button combos for triggering sounds, lights, servos, or MarcDuino actions
- **MP3 Support** – Supports SparkFun MP3 Trigger and other serial MP3 boards
- **MarcDuino Support** – Optional integration for screen, lighting, and panel control
- **Fully documented** – Includes full pinout, tuning guide, combo sheet, and builder's manual

---

## Hardware Requirements

- Arduino Mega 2560
- 2x 6-Channel RC Receivers + 2x RC Transmitters (e.g., FlySky, HotRC)
- Sabertooth 2x32 motor controller (drive)
- SyRen 10 motor controller (dome)
- SparkFun MP3 Trigger (or supported serial MP3 board) *(optional)*
- 5V regulated power for Arduino + receivers
- 12V+ power supply for motors

---

## Wiring Summary

**Drive Controller (A):**
- CH1 → Pin 2 (Turn)
- CH2 → Pin 3 (Forward/Reverse)
- CH3–CH6 → Pins 22, 24, 26, 28

**Dome Controller (B):**
- CH1 → Pin 21 (Dome Turn)
- CH2–CH6 → Pins 23, 25, 27, 29, 31

**Serial Outputs:**
- TX1 (Pin 18) → MP3 Trigger RX
- TX2 (Pin 16) → Sabertooth & SyRen S1 line (daisy-chained)
- TX3 (Pin 14) → MarcDuino RX *(if used)*

---

## DIP Switch Settings

**Sabertooth 2x32 (Drive Motors):**  
`[OFF OFF ON ON ON ON]` → Address 128, Packetized Serial Mode

**SyRen 10 (Dome Motor):**  
`[OFF OFF ON OFF ON ON]` → Address 129, Packetized Serial Mode

---

## Installation

1. Open `Shadow_RC_v1.0.ino` in the Arduino IDE
2. Select **Board**: Arduino Mega or Mega 2560
3. Select the correct **COM port**
4. Upload the code to your Arduino Mega
5. Power on the droid — the onboard LED (pin 13) will blink to indicate the current mode:
   - 1 Blink = Manual Mode
   - 2 = Carpet Mode
   - 3 = Hybrid Mode
   - 4 = Automated Mode

---

## File Overview

| File / Folder | Purpose |
|---------------|---------|
| `/Documentation` | Builder’s Guide, Quick Start, Combo Sheet, Tunable Parameters |
| `/Libraries/Sabertooth` | Sabertooth library used for motor control |
| `ManualMode.cpp` | Code for direct joystick drive + dome control |
| `CarpetMode.cpp` | Enhanced drive for carpet and high-friction terrain |
| `HybridMode.cpp` | Drive + dome automation + MP3 ambient |
| `AutomatedMode.cpp` | Fully autonomous dome and sound |
| `ComboHandler.cpp` | All 32 joystick+button combos mapped here |
| `MP3Handler.cpp` | Sound playback logic and category mapping |
| `PWMInputHandler.cpp` | Interrupt-based PWM reader for all RC channels |

---

## MP3 Categories (Default)

| Category | File Range |
|----------|------------|
| Happy    | 001–016    |
| Sad      | 031–035    |
| Talking  | 061–076    |
| Yelling  | 091–103    |
| Classic  | 121–123    |
| Dance    | 151–156    |
| Singing  | 181–186    |
| Lines    | 211–212    |

*(You can update these ranges in `MP3Handler.cpp` to match your SD card setup. MP3 filenames must use the format `####.mp3`, e.g., `0061.mp3`.)*

---

## Combo System Overview

- 32 unique combos triggered by joystick direction + opposite controller button
- CH3–CH5 are **toggle switches** (trigger on any state change)
- CH6 is a **momentary button** (trigger only on rising edge, ~1988 µs pulse)
- Combos 1–4 = Mode Switching
- Combos 5–8 = MarcDuino Mode Commands
- Combos 9–20 = MP3 categories
- Combos 21–32 = Available for custom use

Refer to the **Combo Reference Sheet** for the full map.

---

## Licensing

This project is open-source for personal use and educational purposes. Attribution is appreciated. Please credit the Shadow-RC project if adapting or sharing.

---

**May the Force be with you, Builder.**
