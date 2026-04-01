#include "NixieHttpServer.h"
#include <esp_http_server.h>
#include "esp_err.h"
#include "esp_log.h"
#include "Connectivity.h"
#include "Storage.h"
#include <ctype.h>

static const char *TAG = "Nixie HTTP";

static httpd_handle_t server = NULL;
const char WIFI_FORM_STR[];

esp_err_t get_wifi_form_handler(httpd_req_t *req);
esp_err_t post_new_wifi_handler(httpd_req_t *req);


/**
 * @brief URL-decode a string in-place.
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

esp_err_t hello_get_handler(httpd_req_t *req)
{
    const char* resp_str = "<h1>Hello World!</h1><p>From Switzerland :)</p>";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

void NixieHttpServer_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();


    ESP_LOGI(TAG, "Web server start on port %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t hello = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = hello_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &hello);

        httpd_uri_t wifi_form = {
            .uri      = "/wifiForm",
            .method   = HTTP_GET,
            .handler  = get_wifi_form_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_form);

        httpd_uri_t new_wifi = {
            .uri      = "/newWifi",
            .method   = HTTP_POST,
            .handler  = post_new_wifi_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &new_wifi);
    }
}





esp_err_t post_new_wifi_handler(httpd_req_t *req) {
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

        // TODO: start STA again.
        Connectivity_NotifyNewCredentials();
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Error: Post SSID and Password");
    }
    return ESP_OK;
}


esp_err_t get_wifi_form_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, WIFI_FORM_STR);
}



const char WIFI_FORM_STR[] =
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<style>"
    "body { font-family: Arial, sans-serif; padding:20px; margin:0; }"
    "form { max-width:400px; margin:auto; display:flex; flex-direction:column; gap:12px; }"
    "input { padding:12px; font-size:16px; border:1px solid #ccc; border-radius:8px; width:100%; box-sizing:border-box; }"
    "input[type=submit] { background:#007bff; color:white; border:none; cursor:pointer; }"
    "input[type=submit]:hover { background:#0056b3; }"
    "</style></head><body>"
    "<h2 style='text-align:center;'>WiFi wonfiguration</h2>"
    "<form method='POST' action='/newWifi'>"
    "<input name='ssid' placeholder='SSID'>"
    "<input name='password' type='password' placeholder='Password'>"
    "<input type='submit' value='Connect'>"
    "</form>"
    "</body></html>";