#include "chef_scale.h"
#include "chef_styles.h"
#include "esp_log.h"
#include "../chef_hx711/HX711.h"
#include "../chef_buttons/chef_button.h"
#include "chef_startup.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#define GPIO_DATA   GPIO_NUM_27
#define GPIO_SCLK   GPIO_NUM_12
#define AVG_SAMPLES   10
#define DEBOUNCE_DELAY     50

static const char *TAG = "SCALE_SCREEN";

lv_obj_t * weight_label;
lv_obj_t * screen_scale;
TaskHandle_t buttonhandle_scale = NULL;

typedef struct {
    lv_obj_t *label;
    char text[16];
} label_update_data_t;

void back_pressed_scale() {
    chef_screen_create_home();
    vTaskDelete(buttonhandle_scale);
    lv_obj_del(screen_scale);
}

static void label_update_cb(void *data) 
{
    label_update_data_t *update = (label_update_data_t *)data;
    lv_label_set_text(update->label, update->text);
    lv_refr_now(NULL);

}

static void weight_reading_task(void* arg)
{
    HX711_init(GPIO_DATA,GPIO_SCLK,eGAIN_128); 
    HX711_tare();
    char weight_str[10]; 
    float weight =0;

    label_update_data_t *update_data = malloc(sizeof(label_update_data_t));
    update_data->label = weight_label;
    while(1)
    {
        weight = HX711_get_units(AVG_SAMPLES);
        // Format with one decimal place
        snprintf(update_data->text, sizeof(update_data->text), "%.1f", weight);
        
        // Queue the update
        lv_async_call(label_update_cb, update_data);
        
        ESP_LOGI(TAG, "******* weight = %f *********\n ", weight);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void initialise_weight_sensor(void)
{
    ESP_LOGI(TAG, "****************** Initialing weight sensor **********");
    xTaskCreatePinnedToCore(weight_reading_task, "weight_reading_task", 4096, NULL, 1, NULL,0);
}

void button_task_scale(void *params) {
    ESP_LOGI(TAG, "Waiting for button press");
    int current_state;
    static bool btn_prev_released = true;

    int64_t current_time = esp_timer_get_time();

    while (1) {
        int64_t next_time = esp_timer_get_time();
        if (next_time > current_time + 1000000) {
            current_state = gpio_get_level(BTN_PREV);
            if (current_state == 0 && btn_prev_released) {
                vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
                if (gpio_get_level(BTN_PREV) == 0) {
                    ESP_LOGI("Button Task", "PREV button pressed");
                    back_pressed_scale();
                    btn_prev_released = false;
                }
            } else if (current_state == 1) {
                btn_prev_released = true;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

lv_obj_t* chef_screen_create_scale() {
    ESP_LOGI(TAG, "Creating Scale Screen");
    screen_scale = lv_obj_create(NULL);
    extern lv_style_t screen_background;
    extern lv_style_t label_style;
    lv_obj_add_style(screen_scale, &screen_background, 0);

    // Create title label
    lv_obj_t * title_label = lv_label_create(screen_scale);
    static lv_style_t title_style;
    lv_style_init(&title_style);
    lv_style_set_text_font(&title_style, &lv_font_montserrat_14);
    lv_style_set_text_color(&title_style, lv_color_hex(0x333333));
    lv_obj_add_style(title_label, &title_style, 0);
    lv_label_set_text(title_label, "Weight");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // Create weight label
    weight_label = lv_label_create(screen_scale);
    lv_obj_add_style(weight_label, &label_style, 0);
    lv_label_set_text(weight_label, "0.0");
    lv_obj_align(weight_label, LV_ALIGN_CENTER, 0, 0);

    // Create unit label
    lv_obj_t *unit_label = lv_label_create(screen_scale);
    static lv_style_t unit_label_style;
    lv_style_init(&unit_label_style);
    lv_style_set_text_font(&unit_label_style, &lv_font_montserrat_14);
    lv_style_set_text_color(&unit_label_style, lv_color_hex(0x333333));
    lv_obj_add_style(unit_label, &unit_label_style, 0);
    lv_label_set_text(unit_label, "g");
    lv_obj_align_to(unit_label, weight_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);


    xTaskCreatePinnedToCore(button_task_scale, "button_task", 8192, NULL, 5, &buttonhandle_scale, 0);
    nvs_flash_init();
    initialise_weight_sensor();

    lv_scr_load(screen_scale);
    lv_refr_now(NULL);

    return screen_scale;
}