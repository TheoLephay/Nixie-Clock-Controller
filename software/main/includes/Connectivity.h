#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

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
    AP_PROVISIONING
} ConnectivityState_t;


void Connectivity_Init(void);

void Connectivity_NotifyNewCredentials(void);


#endif