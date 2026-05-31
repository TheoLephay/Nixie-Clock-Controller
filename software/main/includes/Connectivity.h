#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

#include "esp_event_base.h"

#define WIFI_MAX_RETRY 2
#define WIFI_AP_SSID "ESP32_Nixie"

#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

#define WIFI_STR_LEN 32u

typedef struct {
    char ssid[WIFI_STR_LEN];
    char password[WIFI_STR_LEN];
} WifiCredentials_t;

typedef enum
{
    STA_CONNECTING,
    STA_CONNECTED,
    STA_STOPPING,
    AP_PROVISIONING_SETUP,
    AP_PROVISIONING,
    AP_STOPPING,
} ConnectivityState_t;

typedef struct
{
    esp_event_base_t eventBase;
    int32_t          eventId;
    void*            eventData;
} ConnectivityEvent_t;

typedef enum
{
    CONNECTIVITY_NEW_CREDENTIALS_RECEIVED = 0,
} ConnectivityEventId_t;

ESP_EVENT_DECLARE_BASE(CONNECTIVITY_EVENT);

void Connectivity_Init(void);

void Connectivity_Task(void* arg);

void Connectivity_QueueEvent(int32_t eventId);


#endif