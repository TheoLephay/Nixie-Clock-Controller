#include "Storage.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "Storage";

#define CONNECTIVITY_NAMESPACE "connectivity"
#define SSID_KEY "wifi_ssid"
#define PASS_KEY "wifi_pass"

esp_err_t Storage_GetCredentials(WifiCredentials_t *creds)
{
    esp_err_t err = ESP_OK;
    nvs_handle_t nvsHandle;
    size_t ssidLen = sizeof(creds->ssid);
    size_t passLen = sizeof(creds->password);

    err = nvs_open(CONNECTIVITY_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) return err;

    err = nvs_get_str(nvsHandle, SSID_KEY, creds->ssid, &ssidLen);
    if (err != ESP_OK) goto cleanup;

    err = nvs_get_str(nvsHandle, PASS_KEY, creds->password, &passLen);
    if (err != ESP_OK) goto cleanup;

    ESP_LOGI(TAG, "Wifi credentials found, ssid:%s, password:%s", creds->ssid, creds->password);

cleanup:
    nvs_close(nvsHandle);
    return err;
}

esp_err_t Storage_SaveCredentials(const WifiCredentials_t *creds)
{
    esp_err_t err = ESP_OK;
    nvs_handle_t nvsHandle;

    err = nvs_open(CONNECTIVITY_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvsHandle, SSID_KEY, creds->ssid);
    if (err != ESP_OK) goto cleanup;

    err = nvs_set_str(nvsHandle, PASS_KEY, creds->password);
    if (err != ESP_OK) goto cleanup;

    err = nvs_commit(nvsHandle);

cleanup:
    nvs_close(nvsHandle);
    return err;
}
