#include "esp_log.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "chef_network/chef_wifi.h"
#include "chef_network/chef_client.h"
#include "chef_lvgl/lvgl_setup.h"
#include "lvgl.h"
#include "chef_screens/chef_styles.h"
#include "chef_screens/chef_startup.h"
#include "chef_buttons/chef_button.h"
#include "chef_hx711/HX711.h"


static const char *TAG = "Main file";

void lvgl_handler_task(void *pvParameters) {
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    while (1) {
        lv_timer_handler();
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main() {

    ESP_LOGI(TAG, "Good morning! Device booting up!");
    initialize_wifi();
    ESP_LOGI(TAG, "WIFI has been successfully initialized");
    fetch_github_json();
    ESP_LOGI(TAG, "Recipes have been fetched");

    lvgl_init_all();
    setup_buttons();
    chef_init_styles();
    chef_screen_create_home();
    nvs_flash_init();
    
    xTaskCreatePinnedToCore(lvgl_handler_task, "lvgl_handler", 8192, NULL, 5, NULL, 1);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}