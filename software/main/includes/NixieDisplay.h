#ifndef NIXIEDISPLAY_H
#define NIXIEDISPLAY_H

#include "HV5622.h"

/**
 * Driver for 8 nixie tubes with HV5622 shift registers
 */


#define TUBE_COUNT 8u

// 0-9 + comma
#define DIGIT_PER_TUBE 11u

// Value for comma
#define COMMA 10u

// Value for turning off the tube
#define BLANK DIGIT_PER_TUBE

#define CATHODE_NUMBER (TUBE_COUNT * DIGIT_PER_TUBE)

/**
 * Set the value of the selected tube into memory.
 * @param tubeIdx from 0 to 7. 0 is leftmost tube
 * @param value 0-9 for digits, `COMMA` for comma, `BLANK` for OFF.
 * @return success
 */
bool NixieDisplay_SetTubeValue(uint8_t tubeIdx, uint8_t value);

/**
 * Update harware based on values set by `NixieDisplay_SetTubeValue()`
 */
void NixieDisplay_UpdateDisplay(void);

/**
 * Initialise hardware (gpio, shift registers)
 */
void NixieDisplay_Init(void);



#endif