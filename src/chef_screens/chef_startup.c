#include "chef_startup.h"
#include "esp_log.h"
#include "chef_buttons/chef_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "esp_task_wdt.h"
#include "chef_recipes.h"
#include "chef_timer.h"
#include "chef_scale.h"

#define DEBOUNCE_DELAY 50

static const char *TAG = "HOME_SCREEN";

static int highlighted_button = 0;
TaskHandle_t buttonhandle = NULL;

lv_obj_t* recipes;
lv_obj_t* weight;
lv_obj_t* timer;
lv_obj_t* main_page;

char* dish; 

void recipes_pressed(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* recipe_screen = chef_screen_create_recipe();
        if (recipe_screen) {
            lv_scr_load_anim(recipe_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
        }
        vTaskDelete(buttonhandle);
        lv_obj_del(main_page);
    }
}

void timer_pressed(lv_event_t * e){
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* timer_screen = chef_create_timer_screen();
        if (timer_screen) {
            lv_scr_load_anim(timer_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
        }
        vTaskDelete(buttonhandle);
        lv_obj_del(main_page);
    }
}

void weight_pressed(lv_event_t * e){
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* scale_screen = chef_screen_create_scale();
        if (scale_screen) {
            lv_scr_load_anim(scale_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
        }
        vTaskDelete(buttonhandle);
        lv_obj_del(main_page);
    }
}

void update_button_highlight() {

    ESP_LOGI(TAG,"In update highlight");

    lv_obj_set_style_bg_color(recipes, lv_color_white(), 0);
    lv_obj_set_style_bg_color(weight, lv_color_white(), 0);
    lv_obj_set_style_bg_color(timer, lv_color_white(), 0);
    switch (highlighted_button) {
         case 0:
            ESP_LOGI(TAG,"Case 0");
            lv_obj_set_style_bg_color(recipes, lv_palette_main(LV_PALETTE_RED), 0);
            break;
        case 1:
            ESP_LOGI(TAG,"Case 1");
            lv_obj_set_style_bg_color(weight, lv_palette_main(LV_PALETTE_RED), 0);
            break;
        case 2:
            ESP_LOGI(TAG,"Case 2");
            lv_obj_set_style_bg_color(timer, lv_palette_main(LV_PALETTE_RED), 0);
            break;
    }

    lv_obj_invalidate(main_page);
    lv_refr_now(NULL);

}

void handle_select_press() {
    switch (highlighted_button) {
        case 0:
            ESP_LOGI(TAG,"Recipes selected");
            lv_obj_send_event(recipes, LV_EVENT_CLICKED, NULL);
            break;
        case 1:
            ESP_LOGI(TAG,"Scale selected");
            lv_obj_send_event(weight, LV_EVENT_CLICKED, NULL);
            break;
        case 2:
            ESP_LOGI(TAG,"Timer selected");
            lv_obj_send_event(timer, LV_EVENT_CLICKED, NULL);
            break;
    }
}


void button_task(void *params) {

    ESP_LOGI(TAG, "Waiting for button press");
    int current_state;
    static bool btn_down_released = true;
    static bool btn_up_released = true;
    static bool btn_select_released = true;

    while (1) {
        // Handle BTN_DOWN
        current_state = gpio_get_level(BTN_DOWN);
        if (current_state == 0 && btn_down_released) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
            if (gpio_get_level(BTN_DOWN) == 0) {
                ESP_LOGI("Button Task", "down button pressed");
                highlighted_button = (highlighted_button + 1) % 3;
                update_button_highlight();
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
                ESP_LOGI("Button Task", "Up button pressed");
                highlighted_button = (highlighted_button + 2) % 3;
                update_button_highlight();
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
                handle_select_press();
                btn_select_released = false;
            }
        } else if (current_state == 1) {
            btn_select_released = true;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

lv_obj_t* chef_screen_create_home() {

    ESP_LOGI(TAG, "Creating home screen");
    
    main_page = lv_obj_create(NULL);
    extern lv_style_t screen_background;
    lv_obj_add_style(main_page, &screen_background, 0);
    lv_obj_set_flex_flow(main_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_page, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(main_page, 10, 0);


    recipes = lv_btn_create(main_page);
    lv_obj_set_style_bg_color(recipes, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(recipes, 100, 35);
    lv_obj_align(recipes, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(recipes, recipes_pressed, LV_EVENT_CLICKED, NULL);  // Add an event callback

    lv_obj_t* recipes_label = lv_label_create(recipes);
    lv_label_set_text(recipes_label, "Recipes");
    lv_obj_set_style_text_color(recipes_label, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_align_to(recipes_label, recipes, LV_ALIGN_TOP_MID, 0, 5);   


    weight = lv_btn_create(main_page);
    lv_obj_set_style_bg_color(weight, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(weight, 100, 35);
    lv_obj_align(weight, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(weight, weight_pressed, LV_EVENT_CLICKED, NULL);

    lv_obj_t* weight_label = lv_label_create(weight);
    lv_label_set_text(weight_label, "Scale");
    lv_obj_set_style_text_color(weight_label, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_align_to(weight_label, weight, LV_ALIGN_TOP_MID, 0, 5);   


    timer = lv_btn_create(main_page);
    lv_obj_set_style_bg_color(timer, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(timer, 100, 35);
    lv_obj_align(timer, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(timer, timer_pressed, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* timer_label = lv_label_create(timer);
    lv_label_set_text(timer_label, "Timer");
    lv_obj_set_style_text_color(timer_label, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_align_to(timer_label, timer, LV_ALIGN_TOP_MID, 0, 5);   

    xTaskCreatePinnedToCore(button_task, "button_task", 8192, NULL, 5, &buttonhandle, 0);
    update_button_highlight();
    lv_scr_load(main_page);
    lv_refr_now(NULL);
    return main_page;
}