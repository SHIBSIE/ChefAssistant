#include "chef_startup.h"
#include "esp_log.h"
#include "chef_buttons/chef_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "../chef_network/chef_client.h"
#include "chef_info.h"
#include "esp_timer.h"

#define DEBOUNCE_DELAY 50

static const char *TAG = "RECIPE_SCREEN";

lv_obj_t* recipes_screen;
static int highlighted_button_recipes = 0;
TaskHandle_t buttonhandle_recipes = NULL;
int y_offset = 40;

void recipe_pressed(lv_event_t * e){
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED){
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        dish = lv_label_get_text(label);
        chef_screen_create_info();
        vTaskDelete(buttonhandle_recipes);
    }
}

void back_pressed(){
    chef_screen_create_home();
    vTaskDelete(buttonhandle_recipes);
    lv_obj_del(recipes_screen);
}

void handle_select_press_recipes() {

    ESP_LOGI(TAG,"Recipes selected");
    lv_obj_send_event(lv_obj_get_child(recipes_screen,highlighted_button_recipes), LV_EVENT_CLICKED, NULL);

}

void update_button_highlight_recipes() {

    ESP_LOGI(TAG,"In update highlight");

    lv_obj_set_style_bg_color(lv_obj_get_child(recipes_screen,highlighted_button_recipes-1), lv_color_white(), 0);
    lv_obj_set_style_bg_color(lv_obj_get_child(recipes_screen,highlighted_button_recipes), lv_palette_main(LV_PALETTE_RED), 0);

    lv_obj_invalidate(recipes_screen);
    lv_refr_now(NULL);

}

void button_task_recipes(void *params) {

    ESP_LOGI(TAG, "Waiting for button press");
    int current_state;
    static bool btn_up_released = true;
    static bool btn_down_released = true;
    static bool btn_prev_released = true;
    static bool btn_select_released = true;

    int64_t current_time = esp_timer_get_time();

    while (1) {
        int64_t next_time = esp_timer_get_time();
        if (next_time>current_time+1000000){
        // Handle BTN_NEXT
            current_state = gpio_get_level(BTN_DOWN);
            if (current_state == 0 && btn_down_released) {
                vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
                if (gpio_get_level(BTN_NEXT) == 0) {
                    ESP_LOGI("Button Task", "DOWN button pressed");
                    highlighted_button_recipes = (highlighted_button_recipes + 1) % 2;
                    update_button_highlight_recipes();
                    btn_down_released = false;
                }
            } else if (current_state == 1) {
                btn_down_released = true;
            }

            // Handle BTN_PREV
            current_state = gpio_get_level(BTN_UP);
            if (current_state == 0 && btn_up_released) {
                vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
                if (gpio_get_level(BTN_UP) == 0) {
                    ESP_LOGI("Button Task", "UP button pressed");
                    highlighted_button_recipes = (highlighted_button_recipes - 1) % 2;
                    update_button_highlight_recipes();
                    btn_up_released = false;
                }
            } else if (current_state == 1) {
                btn_up_released = true;
            }

            // Handle BTN_SELECT
            current_state = gpio_get_level(BTN_SELECT);
            if (current_state == 0 && btn_select_released) {
                vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
                if (gpio_get_level(BTN_SELECT) == 0) {
                    ESP_LOGI("Button Task", "Select button pressed");
                    handle_select_press_recipes();
                    btn_select_released = false;
                }
            } else if (current_state == 1) {
                btn_select_released = true;
            }

            current_state = gpio_get_level(BTN_PREV);
            if (current_state == 0 && btn_prev_released) {
                vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
                if (gpio_get_level(BTN_PREV) == 0) {
                    ESP_LOGI("Button Task", "PREV button pressed");
                    highlighted_button_recipes = (highlighted_button_recipes + 1) % 2;
                    back_pressed();
                    btn_prev_released = false;
                }
            } else if (current_state == 1) {
                btn_prev_released = true;
            }

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}


lv_obj_t* chef_screen_create_recipe() {
    ESP_LOGI(TAG, "Creating recipes screen");
    
    recipes_screen = lv_obj_create(NULL);
    extern lv_style_t screen_background;
    lv_obj_add_style(recipes_screen, &screen_background, 0);
    
    cJSON* cjson = fetch_recipe_json();
    if (cjson == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
    }

    cJSON *recipes = cJSON_GetObjectItemCaseSensitive(cjson, "recipes");
    cJSON *recipe;

    int i = 0;
    cJSON_ArrayForEach(recipe, recipes) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(recipe, "name");
        if (cJSON_IsString(name) && (name->valuestring != NULL)) {
            lv_obj_t* btn = lv_btn_create(recipes_screen);
            if (i == 0){
                lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            else{
                lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_size(btn, 100, 35);
            lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, i*y_offset);
            i++;

            lv_obj_add_event_cb(btn, recipe_pressed, LV_EVENT_CLICKED, name->valuestring);  // Add an event callback

            lv_obj_t* btn_label = lv_label_create(btn);
            lv_label_set_text(btn_label, name->valuestring);
            lv_obj_set_style_text_color(btn_label, lv_color_black(), LV_STATE_DEFAULT);
            lv_obj_align_to(btn_label, btn, LV_ALIGN_TOP_MID, 0, 5);
        }
    }

    xTaskCreatePinnedToCore(button_task_recipes, "button_task", 8192, NULL, 5, &buttonhandle_recipes, 0);

    lv_scr_load(recipes_screen);
    lv_refr_now(NULL);

    return recipes_screen;
}