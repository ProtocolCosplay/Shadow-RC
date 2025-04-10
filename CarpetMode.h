/*
  ╔════════════════════════════════════════════════════════════╗
  ║                    CarpetMode.h - Shadow-RC                ║
  ║────────────────────────────────────────────────────────────║
  ║ Header file for Carpet Mode control logic.                 ║
  ║ Declares `setupCarpetMode()` and `loopCarpetMode()` used   ║
  ║ to help R2-D2 navigate high-resistance surfaces like carpet║
  ║ or rough flooring with enhanced torque and responsiveness. ║
  ║                                                            ║
  ║ DO NOT MODIFY unless you're tuning terrain behavior or     ║
  ║ adjusting drive characteristics for non-smooth surfaces.   ║
  ╚════════════════════════════════════════════════════════════╝
*/


#ifndef CARPET_MODE_H
#define CARPET_MODE_H

void setupCarpetMode();
void loopCarpetMode();

#endif
