#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ModeSwitch.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "SmartClock.h"

/////////////////////
/// Macros
/////////////////////

#define DEBOUNCE_DELAY_US 1000u
#define LONG_PRESS_US     500000u


/////////////////////
/// Prototypes
/////////////////////

void ModeSwitch_LongPressCb(void *);

/////////////////////
/// Private typedefs
/////////////////////

struct
{
    uint32_t time;
    bool risingEdge;
} typedef GpioItr_t;

/////////////////////
/// Static data
/////////////////////

TaskHandle_t switchTaskHandle;

static QueueHandle_t gpioEvtQueue = NULL;

esp_timer_handle_t switchTimerHandle = { 0 };
esp_timer_create_args_t switchTimerData = {
    .callback = ModeSwitch_LongPressCb,
    .arg = NULL,
    .name = "switchTimerData",
    .skip_unhandled_events = true,
};


/////////////////////
/// Private functions
/////////////////////

void ModeSwitch_LongPressCb(void *arg)
{
    xTaskNotify(clockTaskHandle, NTF_POWER_TUBES_MSK, eSetBits);
}


void ModeSwitch_GpioItrCb(void *arg)
{

    BaseType_t HigherPriorityTaskWoken = pdFALSE;
    GpioItr_t data = {
        .time = (uint32_t) esp_timer_get_time(),
        .risingEdge = gpio_get_level(MODE_PIN)
    };

    xQueueSendFromISR(gpioEvtQueue, &data, &HigherPriorityTaskWoken);
    portYIELD_FROM_ISR(HigherPriorityTaskWoken);
}


void ModeSwitch_ProcessItr(GpioItr_t gpioItr)
{
    static uint32_t lastPress;
    if (gpioItr.time - lastPress > DEBOUNCE_DELAY_US)
    {
        lastPress = gpioItr.time;
    }
    else
    {
        return;
    }


    if (gpioItr.risingEdge)
    {
        esp_timer_start_once(switchTimerHandle, LONG_PRESS_US);
        xTaskNotify(clockTaskHandle, NTF_CHANGE_MODE_MSK, eSetBits);
    }
    else
    {
        // Running or not, stop it
        esp_timer_stop(switchTimerHandle);
    }
}


void ModeSwitch_Task(void *arg)
{
    GpioItr_t gpioItr;
    while (1)
    {
        if (xQueueReceive(gpioEvtQueue, &gpioItr, portMAX_DELAY))
        {
            ModeSwitch_ProcessItr(gpioItr);
        }
    }
}


void ModeSwitch_init(void)
{
    gpio_config_t io_conf = { 0 };
    io_conf.pin_bit_mask = MODE_PIN_MSK;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(MODE_PIN, ModeSwitch_GpioItrCb, NULL);

    gpioEvtQueue = xQueueCreate(6, sizeof(GpioItr_t));

    esp_timer_create(&switchTimerData, &switchTimerHandle);
}