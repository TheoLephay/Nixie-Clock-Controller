#include "HV5622.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"


void HV5622_LatchEnable(bool en)
{
    gpio_set_level(GPIO_LE, en);
}

void HV5622_BlankLine(bool blank)
{
    gpio_set_level(GPIO_BL, blank);
}

void HV5622_SetClk(bool level)
{
    gpio_set_level(GPIO_CLK, !level);
}

void HV5622_SetDat(bool level)
{
    gpio_set_level(GPIO_DAT, !level);
}

void HV5622_PushBit(bool bit)
{
    HV5622_SetDat(bit);
    esp_rom_delay_us(1u);
    HV5622_SetClk(0);
    esp_rom_delay_us(1u);
    HV5622_SetClk(1);
}


void HV5622_SetAllRegisters(bool level)
{
    HV5622_LatchEnable(true);
    for (size_t i = 0; i < REGISTER_NUMBER * BIT_PER_REGISTER; i++)
    {
        HV5622_PushBit(level);
    }
    HV5622_LatchEnable(false);
}

void HV5622_InitGpio(void)
{
    gpio_set_level(GPIO_CLK, 0);
    gpio_set_level(GPIO_DAT, 0);
    HV5622_BlankLine(1);
    HV5622_LatchEnable(1);

    gpio_set_direction(GPIO_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_DAT, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_LE, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_BL, GPIO_MODE_OUTPUT);

    HV5622_SetAllRegisters(0);
    HV5622_BlankLine(0);
}

