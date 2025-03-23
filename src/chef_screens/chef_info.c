#include "esp_log.h"
#include "chef_buttons/chef_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "esp_task_wdt.h"
#include "chef_info.h"
#include "chef_ingredients.h"
#include "chef_steps.h"
#include "chef_startup.h"
#include "chef_recipes.h"
#include "esp_timer.h"

#define DEBOUNCE_DELAY 50

static const char *TAG = "INFO_SCREEN";

static int highlighted_button = 0;
TaskHandle_t buttonhandle_info = NULL;

lv_obj_t* ingredients;
lv_obj_t* steps;
lv_obj_t* info_page;

void ingredients_pressed(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* recipe_screen = chef_screen_create_ingredients(dish);
        if (recipe_screen) {
            lv_scr_load_anim(recipe_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
        }
        vTaskDelete(buttonhandle_info);
        lv_obj_del(info_page);
    }
}

void steps_pressed(lv_event_t * e){
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* recipe_screen = chef_screen_create_instructions(dish);
        if (recipe_screen) {
            lv_scr_load_anim(recipe_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
        }
        vTaskDelete(buttonhandle_info);
        lv_obj_del(info_page);
    }
}

void update_button_highlight_info() {

    ESP_LOGI(TAG,"In update highlight");

    lv_obj_set_style_bg_color(ingredients, lv_color_white(), 0);
    lv_obj_set_style_bg_color(steps, lv_color_white(), 0);
    switch (highlighted_button) {
         case 0:
            ESP_LOGI(TAG,"Case 0");
            lv_obj_set_style_bg_color(ingredients, lv_palette_main(LV_PALETTE_RED), 0);
            break;
        case 1:
            ESP_LOGI(TAG,"Case 1");
            lv_obj_set_style_bg_color(steps, lv_palette_main(LV_PALETTE_RED), 0);
            break;
    }

    lv_obj_invalidate(info_page);
    lv_refr_now(NULL);

}

void handle_select_press_info() {
    switch (highlighted_button) {
        case 0:
            ESP_LOGI(TAG,"Ingredients selected");
            lv_obj_send_event(ingredients, LV_EVENT_CLICKED, NULL);
            break;
        case 1:
            ESP_LOGI(TAG,"Steps selected");
            lv_obj_send_event(steps, LV_EVENT_CLICKED, NULL);
            break;
    }
}

void back_pressed_info(){
    chef_screen_create_recipe();
    vTaskDelete(buttonhandle_info);
    lv_obj_del(info_page);
}

void button_task_info(void *params) {

    ESP_LOGI(TAG, "Waiting for button press");
    int current_state;
    static bool btn_down_released = true;
    static bool btn_up_released = true;
    static bool btn_select_released = true;
    static bool btn_prev_released = true;

    int64_t current_time = esp_timer_get_time();

    while (1) {
        int64_t next_time = esp_timer_get_time();
        if (next_time>current_time+1000000){
        // Handle BTN_NEXT
            current_state = gpio_get_level(BTN_DOWN);
            if (current_state == 0 && btn_down_released) {
                vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
                if (gpio_get_level(BTN_DOWN) == 0) {
                    ESP_LOGI("Button Task", "DOWN button pressed");
                    highlighted_button = (highlighted_button + 1) % 2;
                    update_button_highlight_info();
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
                    highlighted_button = (highlighted_button + 1) % 2;
                    update_button_highlight_info();
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
                    handle_select_press_info();
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
                    back_pressed_info();
                    btn_prev_released = false;
                }
            } else if (current_state == 1) {
                btn_prev_released = true;
            }

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

lv_obj_t* chef_screen_create_info() {

    ESP_LOGI(TAG, "Creating info screen");

    info_page = lv_obj_create(NULL);
    extern lv_style_t screen_background;
    lv_obj_add_style(info_page, &screen_background, 0);
    lv_obj_set_flex_flow(info_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_page, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(info_page, 10, 0);


    ingredients = lv_btn_create(info_page);
    lv_obj_set_style_bg_color(ingredients, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(ingredients, 100, 35);
    lv_obj_align(ingredients, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(ingredients, ingredients_pressed, LV_EVENT_CLICKED, dish);  // Add an event callback

    lv_obj_t* ingredients_label = lv_label_create(ingredients);
    lv_label_set_text(ingredients_label, "Ingredients");
    lv_obj_set_style_text_color(ingredients_label, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_align_to(ingredients_label, ingredients, LV_ALIGN_TOP_MID, 0, 5);   


    steps = lv_btn_create(info_page);
    lv_obj_set_style_bg_color(steps, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(steps, 100, 35);
    lv_obj_align(steps, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(steps, steps_pressed, LV_EVENT_CLICKED, dish);

    lv_obj_t* steps_label = lv_label_create(steps);
    lv_label_set_text(steps_label, "Steps");
    lv_obj_set_style_text_color(steps_label, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_align_to(steps_label, steps, LV_ALIGN_TOP_MID, 0, 5);

    xTaskCreatePinnedToCore(button_task_info, "button_task", 8192, NULL, 5, &buttonhandle_info, 0);

    lv_scr_load(info_page);
    lv_refr_now(NULL);
    
    return info_page;
}