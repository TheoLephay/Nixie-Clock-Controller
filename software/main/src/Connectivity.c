#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "mdns.h"
#include "Connectivity.h"
#include "SmartClock.h"
#include "esp_err.h"
#include "NixieHttpServer.h"


#define WIFI_SSID      "gas_gas_gaaas" // "Theo"
#define WIFI_PASS      "Tl04Bg&Z$xwz@p" // "yoloswag"


static const char *TAG = "Connectivity";

static esp_netif_t* netif = NULL;

static ConnectivityState_t connectivityState = STA_CONNECTING;


static void Connectivity_TimeUpdateCb(struct timeval *tv)
{
    ESP_LOGI(TAG, "NTC server update");
    xTaskNotify(clockTaskHandle, NTF_NTC_UPDATE_MSK, eSetBits);
}

static void Connectivity_InitSNTP(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = Connectivity_TimeUpdateCb;

    esp_netif_sntp_init(&config);

    setenv("TZ", TIMEZONE, 1);
    tzset();
}

static void Connectivity_StartMdns()
{
    static bool started = false;
    if (started) return;
    started = true;

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("nixie"));
    ESP_ERROR_CHECK(mdns_instance_name_set("Nixie clock"));
    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, NULL, 0));
}

static void connectivity_WifiStartSta()
{
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void connectivity_WifiStartAp()
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = 6,  // 1, 6 or 11 for no overlap
            .password = "",
            .max_connection = 1,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void Connectivity_WifiEventHandler(int32_t event_id, void* event_data)
{
    static int connRetryNum = 0;

    switch (event_id)
    {
        case WIFI_EVENT_STA_START:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            switch (connectivityState)
            {
            case STA_CONNECTING:
                ESP_LOGI(TAG, "Connecting wifi...");
                esp_wifi_connect();
                break;

            default:
                ESP_LOGW(TAG, "Wrong state %u", connectivityState);
                break;
            }
        }
        break;

        case WIFI_EVENT_STA_CONNECTED:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
            switch (connectivityState)
            {
            case STA_CONNECTING:
                connectivityState = STA_CONNECTED;
                connRetryNum = 0;
                break;

            default:
                ESP_LOGW(TAG, "Wrong state %u", connectivityState);
                break;
            }
        }
        break;

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            switch (connectivityState)
            {
                case STA_CONNECTING: // explicit fallthrough
                case STA_CONNECTED:
                {
                    if (connRetryNum < WIFI_MAX_RETRY)
                    {
                        ESP_LOGI(TAG, "Wifi connection retry...");
                        connectivityState = STA_CONNECTING;
                        connRetryNum++;
                        esp_wifi_connect();
                    }
                    else
                    {
                        ESP_LOGI(TAG,"WIFI connection failure, starting AP.");
                        connectivityState = STA_STOPPING;
                        esp_wifi_stop();
                    }
                }
                break;

                default:
                    ESP_LOGW(TAG, "Wrong state %u", connectivityState);
                break;
            }
        }
        break;

        case WIFI_EVENT_STA_STOP:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP");
            switch (connectivityState)
            {
                case STA_STOPPING:
                    connectivityState = AP_PROVISIONING_SETUP;
                    esp_netif_destroy_default_wifi(netif);
                    netif = esp_netif_create_default_wifi_ap();
                    connectivity_WifiStartAp();
                break;

                default:
                    ESP_LOGW(TAG, "Wrong state %u", connectivityState);
                break;
            }
        }
        break;

        case WIFI_EVENT_AP_START:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
            switch (connectivityState)
            {
                case AP_PROVISIONING_SETUP:
                    connectivityState = AP_PROVISIONING;
                break;

                default:
                    ESP_LOGW(TAG, "Wrong state %u", connectivityState);
                break;
            }
        }
        break;

        default:
            ESP_LOGI(TAG, "Unhandled WiFi event, id=%ld", event_id);
        break;
    }
}

static void Connectivity_IpEventHandler(int32_t event_id, void* event_data)
{
    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WIFI connected with IP:" IPSTR, IP2STR(&event->ip_info.ip));

        Connectivity_StartMdns();
        Connectivity_InitSNTP();
        break;

    default:
        break;
    }
}

static void Connectivity_EventHandler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{

    if (event_base == WIFI_EVENT)
    {
        Connectivity_WifiEventHandler(event_id, event_data);
    }
    else if (event_base == IP_EVENT)
    {
        Connectivity_IpEventHandler(event_id, event_data);
    }
    else
    {
        ESP_LOGI(TAG, "Unhandled event base %s\n", event_base);
    }

}


void Connectivity_InitWifi(void)
{
    // Wifi dynamic memory init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &Connectivity_EventHandler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &Connectivity_EventHandler,
                                                        NULL,
                                                        NULL));
}


void Connectivity_Init(void)
{
    // Init NVS, netif & event loop for wifi and sntp
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Start server in both STA and AP mode
    NixieHttpServer_start();

    // Init Wifi memory and handlers
    Connectivity_InitWifi();

    netif = esp_netif_create_default_wifi_sta();

    // Start STA and try to connect. If it fails connecting, AP will be created from the event handler
    connectivity_WifiStartSta();
}