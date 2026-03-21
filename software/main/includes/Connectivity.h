#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

#define WIFI_MAX_RETRY 5
#define WIFI_AP_SSID "ESP_Nixie"

#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

typedef enum
{
    STA_CONNECTING,
    STA_CONNECTED,
    STA_STOPPING,
    AP_PROVISIONING_SETUP,
    AP_PROVISIONING
} ConnectivityState_t;


void Connectivity_Init(void);


#endif