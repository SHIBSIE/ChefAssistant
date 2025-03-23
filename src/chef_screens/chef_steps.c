#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "../chef_network/chef_client.h"
#include "string.h"
#include "../chef_buttons/chef_button.h"
#include "chef_steps.h"
#include "chef_startup.h"
#include "chef_info.h"
#include "esp_timer.h"

#define SCROLL_AMOUNT 25
#define DEBOUNCE_DELAY 50

static const char *TAG = "INSTRUCTIONS_SCREEN";
TaskHandle_t buttonhandle_instructions = NULL;
lv_obj_t* instructions_screen;

void back_pressed_steps(){
    chef_screen_create_info();
    vTaskDelete(buttonhandle_instructions);
    lv_obj_del(instructions_screen);
}

static void scroll_button_handler(int scroll_up) {
    ESP_LOGI(TAG, "In scroll handler");
    lv_coord_t parent_height = lv_obj_get_height(instructions_screen);
    ESP_LOGI(TAG, "Parent height = %ld", parent_height); 
    lv_coord_t scroll_pos_before = lv_obj_get_scroll_y(instructions_screen);
    if (scroll_up && (scroll_pos_before + SCROLL_AMOUNT) < parent_height) {
        ESP_LOGI(TAG, "scrolling up");
        lv_obj_scroll_by(instructions_screen, 0, -SCROLL_AMOUNT, LV_ANIM_OFF);
    } else if (!scroll_up && scroll_pos_before > 0) {
        ESP_LOGI(TAG, "scrolling down");
        lv_obj_scroll_by(instructions_screen, 0, SCROLL_AMOUNT, LV_ANIM_OFF);
    }
    lv_coord_t scroll_pos_after = lv_obj_get_scroll_y(instructions_screen);
    ESP_LOGI(TAG, "Scrolled from %ld to %ld", scroll_pos_before, scroll_pos_after);
    lv_refr_now(NULL);
}

void button_task_instructions(void *params) {

    ESP_LOGI(TAG, "Waiting for button press");
    int current_state;
    static bool btn_down_released = true;
    static bool btn_up_released = true;
    static bool btn_prev_released = true;

    int64_t current_time = esp_timer_get_time();
    
    while (1) {
        int64_t next_time = esp_timer_get_time();
        if (next_time>current_time+1000000) {
            // Handle BTN_NEXT
            current_state = gpio_get_level(BTN_DOWN);
            if (current_state == 0 && btn_down_released) {
                vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
                if (gpio_get_level(BTN_NEXT) == 0) {
                    ESP_LOGI("Button Task", "down button pressed");
                    scroll_button_handler(1);
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
                    ESP_LOGI("Button Task", "up button pressed");
                    scroll_button_handler(0);
                    btn_up_released = false;
                }
            } else if (current_state == 1) {
                btn_up_released = true;
            }

            current_state = gpio_get_level(BTN_PREV);
            if (current_state == 0 && btn_prev_released) {
                vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
                if (gpio_get_level(BTN_PREV) == 0) {
                    ESP_LOGI("Button Task", "PREV button pressed");
                    back_pressed_steps();
                    btn_prev_released = false;
                }
            } else if (current_state == 1) {
                btn_prev_released = true;
            }


            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

lv_obj_t* chef_screen_create_instructions(){
    
    instructions_screen = lv_obj_create(NULL);
    extern lv_style_t screen_background;
    lv_obj_add_style(instructions_screen, &screen_background, 0);
    lv_obj_set_flex_flow(instructions_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(instructions_screen, 10, 0);
    lv_obj_set_flex_align(instructions_screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(instructions_screen, 200, 0);
    lv_obj_set_scroll_dir(instructions_screen, LV_DIR_VER); // Vertical scrolling enabled
    lv_obj_set_scroll_snap_y(instructions_screen, LV_SCROLL_SNAP_CENTER); // Optional snapping
    lv_obj_set_scrollbar_mode(instructions_screen, LV_SCROLLBAR_MODE_AUTO); 

    cJSON* recipe_json  = fetch_recipe_json();
    if (recipe_json == NULL) {
        ESP_LOGE(TAG, "Failed to fetch recipes JSON");
        return NULL;
    }

    cJSON* recipes = cJSON_GetObjectItem(recipe_json, "recipes");
    if (!cJSON_IsArray(recipes)) {
        ESP_LOGE(TAG, "Recipes is not an array");
        cJSON_Delete(recipe_json);
        return NULL;
    }

    cJSON* recipe = NULL;
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, recipes) {
        cJSON* name = cJSON_GetObjectItem(item, "name");
        if (cJSON_IsString(name) && (strcmp(name->valuestring, dish) == 0)) {
            recipe = item;
            break;
        }
    }

    if (recipe == NULL) {
        ESP_LOGE(TAG, "Recipe not found for dish: %s", dish);
        cJSON_Delete(recipe_json);
        return NULL;
    }

    cJSON* instructions = cJSON_GetObjectItem(recipe, "instructions");
    if (!cJSON_IsArray(instructions)) {
        ESP_LOGE(TAG, "Instructions not found or invalid format");
        cJSON_Delete(recipe_json);
        return NULL;
    }

    // Title label
    lv_obj_t* title = lv_label_create(instructions_screen);
    lv_label_set_text(title, dish);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    
    // Create labels for each instruction step
    int step_num = 1;
    cJSON_ArrayForEach(item, instructions) {
        if (cJSON_IsString(item)) {
            char step_label[256];
            snprintf(step_label, sizeof(step_label), "%d. %s", step_num++, item->valuestring);
            
            lv_obj_t* inst_label = lv_label_create(instructions_screen);
            lv_label_set_long_mode(inst_label, LV_LABEL_LONG_WRAP);
            lv_label_set_text(inst_label, step_label);
            lv_obj_set_width(inst_label, lv_pct(90));  // Set width relative to container
            lv_obj_set_style_text_color(inst_label, lv_color_white(), LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(inst_label, &lv_font_montserrat_12, 0);
            
            // Add padding between steps
            lv_obj_set_style_pad_top(inst_label, 5, 0);
            lv_obj_set_style_pad_bottom(inst_label, 5, 0);
        }
    }

    xTaskCreatePinnedToCore(button_task_instructions, "button_task", 8192, NULL, 5, &buttonhandle_instructions, 0);
    lv_scr_load(instructions_screen);
    lv_refr_now(NULL);

    return instructions_screen;
}