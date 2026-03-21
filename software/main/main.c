

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "NixieDisplay.h"
#include "ModeSwitch.h"
#include "SmartClock.h"
#include "Connectivity.h"
#include "driver/gpio.h"


void app_main(void)
{
    printf("Start\n");

    NixieDisplay_Init();
    ModeSwitch_init();
    Clock_Init();
    Connectivity_Init();

    BaseType_t ret = xTaskCreate(
        Clock_Task,
        "Clock task",
        2048,
        NULL,
        2, // lower number lower prio
        &clockTaskHandle
    );
    if (ret != pdPASS){
        printf("main task creation failed");
        esp_restart();
    }

    ret = xTaskCreate(
        ModeSwitch_Task,
        "Switch task",
        2048,
        NULL,
        3,
        &switchTaskHandle
    );
    if (ret != pdPASS){
        printf("main task creation failed");
        esp_restart();
    }
}