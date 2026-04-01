#ifndef WIFI_STORAGE_H
#define WIFI_STORAGE_H

#include "esp_err.h"
#include "Connectivity.h"


esp_err_t Storage_GetCredentials(WifiCredentials_t *creds);

esp_err_t Storage_SaveCredentials(const WifiCredentials_t *creds);

#endif
