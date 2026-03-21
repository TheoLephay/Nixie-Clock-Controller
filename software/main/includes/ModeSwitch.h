#ifndef MODESWITCH_H
#define MODESWITCH_H

/**
 * Driver of the toggle switch.
 * Detects short and long presses, sends events accordingly.
 */

#define MODE_PIN 5u
#define MODE_PIN_MSK (1u << MODE_PIN)

extern TaskHandle_t switchTaskHandle;

void ModeSwitch_init(void);

void ModeSwitch_Task(void *arg);


#endif