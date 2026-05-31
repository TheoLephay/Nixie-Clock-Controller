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
#include "Storage.h"
#include "esp_timer.h"


static const char *TAG = "Connectivity";

static esp_netif_t* netif = NULL;

static ConnectivityState_t connectivityState = STA_CONNECTING;

QueueHandle_t connectivityQueue;

ESP_EVENT_DEFINE_BASE(CONNECTIVITY_EVENT);


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

static void Connectivity_WifiStartSta(const WifiCredentials_t *creds)
{
    wifi_config_t wifiConfig = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strlcpy((char*) wifiConfig.sta.ssid, creds->ssid, sizeof(wifiConfig.sta.ssid));
    strlcpy((char*) wifiConfig.sta.password, creds->password, sizeof(wifiConfig.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void connectivity_WifiStartAp()
{
    wifi_config_t wifiConfig = {
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
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void Connectivity_Reboot()
{
    esp_restart();
}

esp_timer_handle_t rebootTimerHandle = { 0 };
esp_timer_create_args_t rebootTimerData = {
    .callback = Connectivity_Reboot,
    .arg = NULL,
    .name = "rebootTimerData",
    .skip_unhandled_events = true,
};


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
                case STA_CONNECTING: // intended fallthrough
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

        case WIFI_EVENT_AP_STOP:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
            switch (connectivityState)
            {
                case AP_STOPPING:
                {
                    WifiCredentials_t credentials = {0};
                    if (ESP_OK == Storage_GetCredentials(&credentials))
                    {
                        connectivityState = STA_CONNECTING;
                        esp_netif_destroy_default_wifi(netif);
                        netif = esp_netif_create_default_wifi_sta();
                        Connectivity_WifiStartSta(&credentials);
                    }
                    else
                    {
                        ESP_LOGE(TAG, "FATAL: Failed to get credentials after AP provisioning");
                    }
                }
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

static void Connectivity_ConnectivityEventHandler(int32_t event_id)
{
    switch (event_id)
    {
        case CONNECTIVITY_NEW_CREDENTIALS_RECEIVED:
            ESP_LOGI(TAG, "CONNECTIVITY_NEW_CREDENTIALS_RECEIVED");
            switch (connectivityState)
            {
                case AP_PROVISIONING:
                    ESP_LOGI(TAG, "New credentials received, stopping AP and trying to connect...");
                    connectivityState = AP_STOPPING;
                    esp_wifi_stop();
                break;

                default:
                    ESP_LOGW(TAG, "Wrong state %u", connectivityState);
                break;
            }
        break;

        default:
            ESP_LOGI(TAG, "Unhandled connectivity event, id=%ld", event_id);
        break;
    }
}

static void Connectivity_QueueEventInternal(void* arg, esp_event_base_t eventBase,
                                int32_t eventId, void* eventData)
{
    ConnectivityEvent_t msg = {0};
    msg.eventBase = eventBase;
    msg.eventId = eventId;
    msg.eventData = eventData;

    BaseType_t xStatus = xQueueSendToBack(connectivityQueue, &msg, 0);

    if (xStatus != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to post event to queue");
    }
}

void Connectivity_QueueEvent(int32_t eventId)
{
    Connectivity_QueueEventInternal(NULL, CONNECTIVITY_EVENT, eventId, NULL);
}

void Connectivity_InitWifi(void)
{
    // Wifi dynamic memory init
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &Connectivity_QueueEventInternal,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &Connectivity_QueueEventInternal,
                                                        NULL,
                                                        NULL));
}


void Connectivity_Init(void)
{
    connectivityQueue = xQueueCreate(5, sizeof(ConnectivityEvent_t));

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
}

static void Connectivity_Start(void)
{
    WifiCredentials_t credentials = {0};
    if (ESP_OK != Storage_GetCredentials(&credentials))
    {
        ESP_LOGI(TAG, "No wifi credentials found, starting AP for provisioning");
        connectivityState = AP_PROVISIONING_SETUP;
        netif = esp_netif_create_default_wifi_ap();
        connectivity_WifiStartAp();
    }
    else
    {
        ESP_LOGI(TAG, "Try connection with saved credentials");
        connectivityState = STA_CONNECTING;
        // Start STA and try to connect. If it fails connecting, AP will be created from the event handler
        netif = esp_netif_create_default_wifi_sta();
        Connectivity_WifiStartSta(&credentials);
    }
}


void Connectivity_Task(void* arg)
{
    ConnectivityEvent_t msg;

    Connectivity_Start();

    while (1)
    {
        BaseType_t xStatus = xQueueReceive(connectivityQueue, &msg, portMAX_DELAY);
        if (xStatus == pdFAIL)
        {
            continue;
        }

        if (msg.eventBase == WIFI_EVENT)
        {
            Connectivity_WifiEventHandler(msg.eventId, msg.eventData);
        }
        else if (msg.eventBase == IP_EVENT)
        {
            Connectivity_IpEventHandler(msg.eventId, msg.eventData);
        }
        else if (msg.eventBase == CONNECTIVITY_EVENT)
        {
            Connectivity_ConnectivityEventHandler(msg.eventId);
        }
        else
        {
            ESP_LOGI(TAG, "Unhandled event base %s\n", msg.eventBase);
        }
    }
}