#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>
#include "chef_client.h"
#include "esp_crt_bundle.h"

#define MAX_HTTP_OUTPUT_BUFFER 2048

static const char *TAG = "HTTP_CLIENT";

// Buffer to store response
char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
int output_len = 0;
cJSON* recipes_json;

cJSON* fetch_recipe_json(){
    return recipes_json;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (output_len + evt->data_len < MAX_HTTP_OUTPUT_BUFFER) {
                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
                    output_len += evt->data_len;
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

void fetch_github_json()
{
    memset(output_buffer, 0, MAX_HTTP_OUTPUT_BUFFER);
    output_len = 0;
    
    esp_http_client_config_t config = {
        .url = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", //replace with URL
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d", esp_http_client_get_status_code(client));
        ESP_LOGI(TAG, "Content length = %d", output_len);
        ESP_LOGI(TAG, "Raw response: %s", output_buffer);
        
        recipes_json = cJSON_Parse(output_buffer);
        if (recipes_json != NULL) {
            char* printed = cJSON_Print(recipes_json);
            if (printed) {
                ESP_LOGI(TAG, "Parsed JSON: %s", printed);
                free(printed);
            } else {
                ESP_LOGE(TAG, "Failed to print JSON");
            }
        } else {
            ESP_LOGE(TAG, "JSON Parse Error: %s", cJSON_GetErrorPtr());
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
}