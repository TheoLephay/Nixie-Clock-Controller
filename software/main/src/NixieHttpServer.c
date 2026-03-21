#include "NixieHttpServer.h"
#include <esp_http_server.h>
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "Nixie HTTP";

static httpd_handle_t server = NULL;


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
    }
}