#ifndef SMARTCLOCK_H
#define SMARTCLOCK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern TaskHandle_t clockTaskHandle;

#define NTF_SWITCH_SHORT_PRESS 0u
#define NTF_SWITCH_LONG_PRESS  1u
#define NTF_NTC_UPDATE         2u
#define NTF_MAINTENANCE        3u
#define NTF_TURN_ON            4u
#define NTF_TURN_OFF           5u

#define NTF_SWITCH_SHORT_PRESS_MSK (1u << NTF_SWITCH_SHORT_PRESS)
#define NTF_SWITCH_LONG_PRESS_MSK  (1u << NTF_SWITCH_LONG_PRESS)
#define NTF_NTC_UPDATE_MSK         (1u << NTF_NTC_UPDATE)
#define NTF_MAINTENANCE_MSK        (1u << NTF_MAINTENANCE)
#define NTF_TURN_ON_MSK            (1u << NTF_TURN_ON)
#define NTF_TURN_OFF_MSK           (1u << NTF_TURN_OFF)

// Ratio is 0.2s:60s. TODO make this smarter smarter
#define MAINTENANCE_DURATION_S 1u
#define MAINTENANCE_INTERVAL_S (MAINTENANCE_DURATION_S * 300)

#define SCHEDULE_POINTS_PER_DAY 4u
#define DAYS_PER_WEEK           7u
#define UNDEF_HOUR              24u

typedef enum
{
    CLOCK_STATE_POWER_OFF,
    CLOCK_STATE_WAIT_TIME,  // Wait for real time update from NTP server (on boot)
    CLOCK_STATE_TIME,       // Display current time
    CLOCK_STATE_ROLL,       // Display current time
    CLOCK_STATE_RANDOM,     // Display random digits
    CLOCK_STATE_STEINS,     // Steins gate animation
    CLOCK_STATE_STATE_MAX
} ClockState_t;


void Clock_Task(void *arg);
void Clock_Init(void);


#endif