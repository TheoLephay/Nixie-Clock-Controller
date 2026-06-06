#include "NixieHttpServer.h"
#include <esp_http_server.h>
#include "esp_err.h"
#include "esp_log.h"
#include "Connectivity.h"
#include "Storage.h"
#include "SmartClock.h"
#include <ctype.h>

static const char *TAG = "Nixie HTTP";

static httpd_handle_t server = NULL;
const char WIFI_FORM_STR[];
const char ROOT_HTML[];

esp_err_t NixieHttpServer_GetWifiFormHandler(httpd_req_t *req);
esp_err_t NixieHttpServer_NewWifiHandler(httpd_req_t *req);
esp_err_t NixieHttpServer_ClockModeHandler(httpd_req_t *req);


/**
 * @brief URL-decode a string in-place (AI-Generated).
 * Replaces %XX sequences with their corresponding characters,
 * and '+' with spaces.
 * @param str The string to decode (modified in-place).
 * @return The decoded string (same pointer).
 */
static char* url_decode(char *str) {
    if (!str) return NULL;

    char *p = str, *q = str;
    while (*p) {
        if (*p == '%') {
            if (isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
                char hex[3] = {p[1], p[2], '\0'};
                *q++ = (char)strtol(hex, NULL, 16);
                p += 3;
            } else {
                // Invalid %XX, copy as-is
                *q++ = *p++;
            }
        } else if (*p == '+') {
            *q++ = ' ';
            p++;
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';
    return str;
}

esp_err_t NixieHttpServer_RootHandler(httpd_req_t *req)
{    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, ROOT_HTML, strlen(ROOT_HTML));
}

void NixieHttpServer_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "Web server start on port %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t hello = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = NixieHttpServer_RootHandler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &hello);

        httpd_uri_t wifi_form = {
            .uri      = "/wifiform",
            .method   = HTTP_GET,
            .handler  = NixieHttpServer_GetWifiFormHandler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_form);

        httpd_uri_t new_wifi = {
            .uri      = "/newwifi",
            .method   = HTTP_POST,
            .handler  = NixieHttpServer_NewWifiHandler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &new_wifi);

        httpd_uri_t clock_mode = {
            .uri      = "/clockmode",
            .method   = HTTP_POST,
            .handler  = NixieHttpServer_ClockModeHandler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &clock_mode);
    }
}


esp_err_t NixieHttpServer_NewWifiHandler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf)));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    WifiCredentials_t wifi_cred;
    httpd_resp_set_type(req, "text/plain");
    if (httpd_query_key_value(buf, "ssid", wifi_cred.ssid, sizeof(wifi_cred.ssid)) == ESP_OK &&
        httpd_query_key_value(buf, "password", wifi_cred.password, sizeof(wifi_cred.password)) == ESP_OK) {

        url_decode(wifi_cred.ssid);
        url_decode(wifi_cred.password);
        Storage_SaveCredentials(&wifi_cred);

        httpd_resp_sendstr(req, "Got new credentials, trying to connect...");

        Connectivity_QueueEvent(CONNECTIVITY_NEW_CREDENTIALS_RECEIVED);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Error: Post SSID and Password");
    }
    return ESP_OK;
}


esp_err_t NixieHttpServer_ClockModeHandler(httpd_req_t *req) {
    xTaskNotify(clockTaskHandle, NTF_CHANGE_MODE_MSK, eSetBits);
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t NixieHttpServer_GetWifiFormHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, WIFI_FORM_STR);
}


const char WIFI_FORM_STR[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<style>"
        "body {"
            "font-family: Arial, sans-serif;"
            "padding: 20px;"
            "margin: 0;"
        "}"
        "form {"
            "max-width: 400px;"
            "margin: auto;"
            "display: flex;"
            "flex-direction: column;"
            "gap: 12px;"
        "}"
        "input {"
            "padding: 12px;"
            "font-size: 16px;"
            "border: 1px solid #ccc;"
            "border-radius: 8px;"
            "width: 100%;"
            "box-sizing: border-box;"
        "}"
        "input[type=submit] {"
            "background: #007bff;"
            "color: white;"
            "border: none;"
            "cursor: pointer;"
        "}"
        "input[type=submit]:hover {"
            "background: #0056b3;"
        "}"
        ".back-button {"
            "width: 100px;"
            "padding: 8px 12px;"
            "font-size: 14px;"
            "margin: 0;"
        "}"
        ".back-form {"
            "max-width: none;"
            "margin: 0;"
            "width: fit-content;"
        "}"
    "</style>"
"</head>"
"<body>"
    "<form method='GET' action='/' class='back-form'>"
        "<input type='submit' value='Back' class='back-button'>"
    "</form>"
    "<h2 style='text-align:center;'>WiFi Configuration</h2>"
    "<form method='POST' action='/newwifi'>"
        "<input name='ssid' placeholder='SSID'>"
        "<input name='password' type='password' placeholder='Password'>"
        "<input type='submit' value='Connect'>"
    "</form>"
"</body>"
"</html>";

const char ROOT_HTML[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<style>"
        "body {"
            "font-family: Arial, sans-serif;"
            "padding: 20px;"
            "margin: 0;"
        "}"
        "form {"
            "max-width: 400px;"
            "margin: auto;"
            "display: flex;"
            "flex-direction: column;"
            "gap: 12px;"
            "padding:12px;"
        "}"
        "input {"
            "padding: 12px;"
            "font-size: 16px;"
            "border: 10px solid #a81212;"
            "border-radius: 8px;"
            "width: 100%;"
        "}"
        "input[type=submit] {"
            "background: #007bff;"
            "color: white;"
            "border: none;"
        "}"
        "input[type=submit]:hover {"
            "background: #0056b3;"
        "}"
    "</style>"
"</head>"
"<body>"
    "<h2 style='text-align:center;'>Nixie Clock</h2>"
    "<form method='GET' action='/wifiform'>"
        "<input type='submit' value='Change Wifi Settings'>"
    "</form>"
    "<form method='POST' action='/clockmode'>"
        "<input type='submit' value='Change Clock mode'>"
    "</form>"
"</body>"
"</html>";