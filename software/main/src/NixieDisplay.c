#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "NixieDisplay.h"
#include <stdint.h>
#include <stdbool.h>
#include "HV5622.h"

// I accidentally broke a pin of the 1st shift register.
// The workaround is to connect a wire from the corresponding pad to the
// next unused pin of the last shift register. Remove this define for new boards.
#define WORKAROUND

// State of each cathode (idx 0 for comma of leftmost tube, idx 1 for digit 0...).
// Maps to hardware wiring.
static bool nixieState[CATHODE_NUMBER] = { 0 };



bool NixieDisplay_SetTubeValue(uint8_t tubeIdx, uint8_t value)
{
    if (tubeIdx >= TUBE_COUNT)
    {
        return false;
    }

    // Erase previous value
    for (uint8_t i = 0; i < DIGIT_PER_TUBE; i++)
    {
        nixieState[tubeIdx * DIGIT_PER_TUBE + i] = false;
    }

    if (value >= DIGIT_PER_TUBE)
    {
        return true;
    }

    // set new value
    switch (value)
    {
    case 10:
        nixieState[tubeIdx * DIGIT_PER_TUBE] = true;
        break;

    default:
        nixieState[tubeIdx * DIGIT_PER_TUBE + value + 1] = true;
        break;
    }
    return true;
}


void NixieDisplay_UpdateDisplay(void)
{
    HV5622_LatchEnable(true);
#ifdef WORKAROUND
    HV5622_PushBit(nixieState[31]);
#endif
    for (size_t i = 0; i < CATHODE_NUMBER; i++)
    {
        HV5622_PushBit(nixieState[CATHODE_NUMBER - i - 1]);
    }
    HV5622_LatchEnable(false);
}

void NixieDisplay_Init(void)
{
    HV5622_InitGpio();
    for (uint8_t tube = 0; tube < TUBE_COUNT; tube++)
    {
        NixieDisplay_SetTubeValue(tube, BLANK);
    }
    NixieDisplay_UpdateDisplay();
}