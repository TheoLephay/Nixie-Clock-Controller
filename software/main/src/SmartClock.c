#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "SmartClock.h"
#include "NixieDisplay.h"
#include <time.h>
#include <sys/time.h>

const char TAG[] = "Clock";
TaskHandle_t clockTaskHandle;

/////// Clock state management ///////
ClockState_t clockState = CLOCK_STATE_POWER_OFF;

const uint32_t StateTickRateMs[CLOCK_STATE_STATE_MAX] =
{
    [CLOCK_STATE_POWER_OFF] = portMAX_DELAY,
    [CLOCK_STATE_WAIT_TIME] = 200u,
    [CLOCK_STATE_TIME]      = 1000u,
    [CLOCK_STATE_RANDOM]    = 100u,
    [CLOCK_STATE_STEINS]    = 50u,
    [CLOCK_STATE_ROLL]      = 400u,
};

/////// CLOCK_STATE_WAIT_TIME data ///////

uint8_t dotPosition = 0u;

/////// MAINTENANCE data ///////

static void Clock_MaintenanceTimerCb(void *);
uint8_t maintenanceDigit = 0;
esp_timer_handle_t maintenanceTimerHandle = { 0 };
esp_timer_create_args_t maintenanceTimerData = {
    .callback = Clock_MaintenanceTimerCb,
    .arg = NULL,
    .name = "maintenanceTimerData",
    .skip_unhandled_events = true,
};
uint32_t counters[TUBE_COUNT][DIGIT_PER_TUBE] = {0};

/////// CLOCK_STATE_STEINS data ///////

uint16_t steinsIteration = 0u;
bool steinsConverged = false;
uint8_t convergenceMask = 0u;
const uint8_t convergenceInterval = 5u;

/////// ON/OFF schedule data ///////

uint8_t schedule[7][SCHEDULE_POINTS_PER_DAY] = {
    {8, 23, UNDEF_HOUR, UNDEF_HOUR},   // Sunday
    {7, 8, 17, 23},
    {7, 8, 17, 23},
    {7, 8, 17, 23},
    {7, 8, 17, 23},
    {7, 8, 17, 23},
    {8, 23, UNDEF_HOUR, UNDEF_HOUR},
};

static void Clock_SchedulerTimerCb(void *);
uint8_t schedulerDigit = 0;
esp_timer_handle_t schedulerTimerHandle = { 0 };
esp_timer_create_args_t schedulerTimerData = {
    .callback = Clock_SchedulerTimerCb,
    .arg = NULL,
    .name = "schedulerTimerData",
    .skip_unhandled_events = true,
};


static uint32_t xorshift32(void)
{
    static uint32_t x = 0xd8961d92;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static void Clock_MaintenanceTimerCb(void *arg)
{
    ESP_LOGI(TAG, "MAINTENANCE TIMER");
    xTaskNotify(clockTaskHandle, NTF_MAINTENANCE_MSK, eSetBits);
}

void Clock_SchedulerTimerCb(void *)
{
    struct tm bdTime;
    time_t now;
    time(&now);
    localtime_r(&now, &bdTime);

    // Determine the current state
    bool state = false;

    if ((bdTime.tm_hour >= schedule[bdTime.tm_wday][0] && bdTime.tm_hour < schedule[bdTime.tm_wday][1]) ||
        (bdTime.tm_hour >= schedule[bdTime.tm_wday][2] && bdTime.tm_hour < schedule[bdTime.tm_wday][3])) {
        state = true;
    }

    xTaskNotify(clockTaskHandle, state ? NTF_TURN_ON_MSK : NTF_TURN_OFF_MSK, eSetBits);

    // Start timer for next hour or next day
    int nextToggleHour = UNDEF_HOUR;
    for (int i = 0; i < SCHEDULE_POINTS_PER_DAY; i++) {
        if (schedule[bdTime.tm_wday][i] > bdTime.tm_hour && schedule[bdTime.tm_wday][i] < UNDEF_HOUR) {
            nextToggleHour = schedule[bdTime.tm_wday][i];
            break;
        }
    }

    int delaySec;
    if (nextToggleHour < UNDEF_HOUR) {
        // Next event is later today
        delaySec = ((nextToggleHour - bdTime.tm_hour) * 3600) - (bdTime.tm_min * 60) - bdTime.tm_sec;
    } else {
        // trigger at midnight to re-evaluate for tomorrow
        delaySec = ((24 - bdTime.tm_hour) * 3600) - (bdTime.tm_min * 60) - bdTime.tm_sec;
    }
    delaySec++; // margin of 1sec for XTAL drift

    esp_timer_start_once(schedulerTimerHandle, (uint64_t)delaySec * 1000000);
}


static void Clock_DoMaintenance(void)
{
    if (clockState == CLOCK_STATE_POWER_OFF)
    {
        return;
    }

    for (size_t digit = 0; digit < DIGIT_PER_TUBE; digit++)
    {
        for (size_t tube = 0; tube < TUBE_COUNT; tube++)
        {
            NixieDisplay_SetTubeValue(tube, digit);
        }
        NixieDisplay_UpdateDisplay();
        vTaskDelay(pdMS_TO_TICKS(MAINTENANCE_DURATION_S * 1000u));
    }

}


static void Clock_AllOff(void)
{
    for (size_t i = 0; i < TUBE_COUNT; i++)
    {
        NixieDisplay_SetTubeValue(i, BLANK);
    }
    NixieDisplay_UpdateDisplay();
}

static void Clock_TickSteinsGate(void)
{
    uint32_t rd;
    // index of next tube that converges
    static uint8_t convergenceIdx = 0u;

    if (steinsConverged)
    {
        return;
    }

    // if multiple of interval, make a number converge
    if (((steinsIteration % convergenceInterval) == 0u) && (steinsIteration > 0u))
    {
        convergenceMask |= (1u << convergenceIdx);
        convergenceIdx++;
    }

    for (uint8_t tubeIdx = 0; tubeIdx < TUBE_COUNT; tubeIdx++)
    {
        if ((convergenceMask & (1u << tubeIdx)) == 0u)
        {
            rd = xorshift32();
            NixieDisplay_SetTubeValue(tubeIdx, rd % 10);
        }
    }

    NixieDisplay_UpdateDisplay();

    if (convergenceMask == 0xFF)
    {
        steinsConverged = true;
        convergenceIdx = 0;
        ESP_LOGI(TAG, "Universe line converged");
    }

    steinsIteration++;
}

static void Clock_StartSteinsGate()
{
    steinsConverged = false;
    steinsIteration = 0u;

    // Tube 1 always displays a comma
    convergenceMask = (1u << 1u);
    NixieDisplay_SetTubeValue(1u, COMMA);
}


static void Clock_Tick(void)
{
    static uint8_t counter = 0;
    // ESP_LOGI(TAG, "update state %d", clockState);
    switch (clockState)
    {
    case CLOCK_STATE_POWER_OFF:
        Clock_AllOff();
        break;

    case CLOCK_STATE_WAIT_TIME:
        for (uint8_t tube = 0; tube < TUBE_COUNT; tube++)
        {
            if (dotPosition == tube)
            {
                NixieDisplay_SetTubeValue(tube, COMMA);
            }
            else
            {
                NixieDisplay_SetTubeValue(tube, BLANK);
            }
        }
        NixieDisplay_UpdateDisplay();
        dotPosition = (dotPosition + 1) % TUBE_COUNT;
        break;

    case CLOCK_STATE_TIME:
        {
            struct tm bdTime;
            struct timeval tv;
            gettimeofday(&tv, NULL);
            time_t now = {tv.tv_sec};
            localtime_r(&now, &bdTime);

            uint8_t tube = 0;
            NixieDisplay_SetTubeValue(tube++, bdTime.tm_hour / 10);
            NixieDisplay_SetTubeValue(tube++, bdTime.tm_hour % 10);
            NixieDisplay_SetTubeValue(tube++, COMMA);
            NixieDisplay_SetTubeValue(tube++, bdTime.tm_min / 10);
            NixieDisplay_SetTubeValue(tube++, bdTime.tm_min % 10);
            NixieDisplay_SetTubeValue(tube++, COMMA);
            NixieDisplay_SetTubeValue(tube++, bdTime.tm_sec / 10);
            NixieDisplay_SetTubeValue(tube++, bdTime.tm_sec % 10);
            NixieDisplay_UpdateDisplay();

            if (tv.tv_usec > 100000) // if more than 100ms delta, resynchronize this task's main loop
            {
                vTaskDelay(pdMS_TO_TICKS(5 + (1000000 - tv.tv_usec) / 1000));
            }
        }
        break;

    case CLOCK_STATE_RANDOM:
        {
            for (size_t tube = 0; tube < TUBE_COUNT; tube++)
            {
                uint32_t rd = xorshift32();
                NixieDisplay_SetTubeValue(tube, rd % DIGIT_PER_TUBE);
            }
            NixieDisplay_UpdateDisplay();
        }
        break;

    case CLOCK_STATE_STEINS:
        Clock_TickSteinsGate();
        break;


    case CLOCK_STATE_ROLL:
        for (size_t tube = 0; tube < TUBE_COUNT; tube++)
        {
            NixieDisplay_SetTubeValue(tube, counter % DIGIT_PER_TUBE);
        }
        counter ++;
        NixieDisplay_UpdateDisplay();

        break;

    default:
        break;
    }
}

static void Clock_ModeChange(void)
{
    ESP_LOGI(TAG, "MODE CHANGE");

    // Rerun steins gate mode if it has converged
    if (clockState == CLOCK_STATE_STEINS)
    {
        if (steinsConverged)
        {
            Clock_StartSteinsGate();
            return;
        }
    }

    // Rotate modes
    if (clockState != CLOCK_STATE_POWER_OFF)
    {
        clockState++;
        if (clockState == CLOCK_STATE_STATE_MAX)
        {
            clockState = CLOCK_STATE_TIME;
        }
        if (clockState == CLOCK_STATE_STEINS)
        {
            Clock_StartSteinsGate();
        }

    }
}

void Clock_Init(void)
{
    esp_timer_create(&maintenanceTimerData, &maintenanceTimerHandle);
    esp_timer_start_periodic(maintenanceTimerHandle, 1000u * 1000u * MAINTENANCE_INTERVAL_S);

    esp_timer_create(&schedulerTimerData, &schedulerTimerHandle);

    clockState = CLOCK_STATE_WAIT_TIME;
    Clock_Tick();
}


void Clock_Task(void *arg)
{
    uint32_t notificationFlags;

    while (1)
    {
        BaseType_t notifReceived = xTaskNotifyWait(0x0u, 0xFFFFFFFFu, &notificationFlags, pdMS_TO_TICKS(StateTickRateMs[clockState]));

        if (notifReceived)
        {
            if (notificationFlags & NTF_CHANGE_MODE_MSK)
            {
                Clock_ModeChange();
            }

            if (notificationFlags & NTF_POWER_TUBES_MSK)
            {
                if (clockState != CLOCK_STATE_POWER_OFF)
                {
                    clockState = CLOCK_STATE_POWER_OFF;
                }
                else
                {
                    clockState = CLOCK_STATE_TIME;
                }
            }

            if (notificationFlags & NTF_TURN_OFF_MSK)
            {
                if (clockState != CLOCK_STATE_POWER_OFF)
                {
                    clockState = CLOCK_STATE_POWER_OFF;
                }
            }

            if (notificationFlags & NTF_TURN_ON_MSK)
            {
                if (clockState == CLOCK_STATE_POWER_OFF)
                {
                    clockState = CLOCK_STATE_TIME;
                }
            }

            if (notificationFlags & NTF_NTC_UPDATE_MSK)
            {
                if (clockState == CLOCK_STATE_WAIT_TIME)
                {
                    clockState = CLOCK_STATE_TIME;
                }
                Clock_SchedulerTimerCb(NULL); // start scheduler
            }

            if (notificationFlags & NTF_MAINTENANCE_MSK)
            {
                // Blocking
                Clock_DoMaintenance();
            }
        }
        Clock_Tick();
    }
}
