#ifndef HV5622_H
#define HV5622_H

#include "stdbool.h"

#define GPIO_DAT 1u
#define GPIO_CLK 0u
#define GPIO_LE  3u
#define GPIO_BL  4u

#define GPIO_MSK_CLK (1u << GPIO_CLK)
#define GPIO_MSK_DAT (1u << GPIO_DAT)
#define GPIO_MSK_LE  (1u << GPIO_LE)
#define GPIO_MSK_BL  (1u << GPIO_BL)

#define REGISTER_NUMBER  3u
#define BIT_PER_REGISTER 32u


void HV5622_InitGpio(void);

void HV5622_LatchEnable(bool en);

void HV5622_BlankLine(bool blank);

void HV5622_PushBit(bool bit);

void HV5622_SetAllRegisters(bool level);


#endif