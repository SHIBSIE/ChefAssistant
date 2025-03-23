#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "chef_startup.h"
#include "chef_timer.h"
#include "../chef_buttons/chef_button.h"

#define TAG                 "TIMER_SCREEN"
#define TIMER_PERIOD_MS    5000
#define DEBOUNCE_DELAY     50
#define MAX_TIME_SECONDS   3600    // 1 hour
#define TIME_STEP_SECONDS  30      // 30-second increments

typedef struct {
    lv_obj_t *spinbox;
    lv_obj_t *start_btn;
    lv_obj_t *timer_label;
} TimerUI;

typedef struct {
    uint32_t set_time_seconds;
    uint32_t remaining_time;
    bool is_running;
    bool is_expired;
} TimerState;

static TimerUI ui;
static TimerState state = {0};
static gptimer_handle_t gptimer = NULL;
static TaskHandle_t buttonhandle_timer = NULL;
lv_obj_t *timer_screen;

static void timer_control(bool start) {
    if (start && !state.is_running) {
        ESP_LOGI(TAG, "Starting timer");
        // Get the current value from the spinbox
        state.set_time_seconds = lv_spinbox_get_value(ui.spinbox);
        
        if (state.set_time_seconds > 0) {
            // Configure the hardware timer
            uint64_t alarm_value = (uint64_t)state.set_time_seconds * 1000000; // Convert to microseconds
            ESP_ERROR_CHECK(gptimer_set_raw_count(gptimer, 0));
            
            gptimer_alarm_config_t alarm_config = {
                .alarm_count = alarm_value,
                .flags.auto_reload_on_alarm = false
            };
            ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
            
            // Start the hardware timer
            ESP_ERROR_CHECK(gptimer_start(gptimer));
            
            // Update UI and state
            state.remaining_time = state.set_time_seconds;
            state.is_running = true;
            state.is_expired = false;
            
            // Update button appearance
            lv_obj_set_style_bg_color(ui.start_btn, lv_color_make(255, 0, 0), 0);
            lv_obj_t *btn_label = lv_obj_get_child(ui.start_btn, 0);
            lv_label_set_text(btn_label, "Stop");
            
            ESP_LOGI(TAG, "Timer started for %lu seconds", state.set_time_seconds);
        } else {
            ESP_LOGW(TAG, "Cannot start timer with 0 seconds");
        }
    } else if (!start && state.is_running) {
        ESP_LOGI(TAG, "Stopping timer");
        
        // Stop the hardware timer
        ESP_ERROR_CHECK(gptimer_stop(gptimer));
        
        // Reset state
        state.is_running = false;
        state.remaining_time = 0;
        
        // Update button appearance
        lv_obj_set_style_bg_color(ui.start_btn, lv_color_make(0, 255, 0), 0);
        lv_obj_t *btn_label = lv_obj_get_child(ui.start_btn, 0);
        lv_label_set_text(btn_label, "Start");
        
        ESP_LOGI(TAG, "Timer stopped");
    }
    
    // Refresh the display
    lv_refr_now(NULL);
}

bool IRAM_ATTR timer_callback(gptimer_handle_t timer, 
                            const gptimer_alarm_event_data_t *event, 
                            void *arg) {
    state.is_expired = true;
    return false;
}

static void timer_init(void) {
    ESP_LOGI(TAG, "Initializing hardware timer");
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000 * 1000
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_LOGI(TAG, "Hardware timer initialization complete");
}

void back_pressed_timer(){
    chef_screen_create_home();
    vTaskDelete(buttonhandle_timer);
    lv_obj_del(timer_screen);
}


static void button_handler_task(void *params) {
    ESP_LOGI(TAG, "Button handler task initialized");
    static bool button_states[4] = {true, true, true, true}; // Released states for NEXT, PREV, SELECT

    while (1) {
        // Handle BTN_NEXT (increment)
        if (gpio_get_level(BTN_UP) == 0 && button_states[0]) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
            if (gpio_get_level(BTN_UP) == 0) {
                uint32_t current_value = lv_spinbox_get_value(ui.spinbox);
                ESP_LOGI(TAG, "Next button pressed, current value: %lu", current_value);
                if (current_value < MAX_TIME_SECONDS) {
                    lv_spinbox_increment(ui.spinbox);
                    ESP_LOGI(TAG, "Incremented to: %lu", lv_spinbox_get_value(ui.spinbox));
                    lv_refr_now(NULL);
                } else {
                    ESP_LOGW(TAG, "Max time limit reached: %d", MAX_TIME_SECONDS);
                }
                button_states[0] = false;
            }
        } else if (gpio_get_level(BTN_UP) == 1) {
            button_states[0] = true;
        }

        // Handle BTN_PREV (decrement)
        if (gpio_get_level(BTN_DOWN) == 0 && button_states[1]) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
            if (gpio_get_level(BTN_DOWN) == 0) {
                uint32_t current_value = lv_spinbox_get_value(ui.spinbox);
                ESP_LOGI(TAG, "Prev button pressed, current value: %lu", current_value);
                if (current_value > 0) {
                    lv_spinbox_decrement(ui.spinbox);
                    ESP_LOGI(TAG, "Decremented to: %lu", lv_spinbox_get_value(ui.spinbox));
                    lv_refr_now(NULL);
                } else {
                    ESP_LOGW(TAG, "Min time limit reached: 0");
                }
                button_states[1] = false;
            }
        } else if (gpio_get_level(BTN_DOWN) == 1) {
            button_states[1] = true;
        }

        // Handle BTN_SELECT (start/stop)
        if (gpio_get_level(BTN_SELECT) == 0 && button_states[2]) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
            if (gpio_get_level(BTN_SELECT) == 0) {
                ESP_LOGI(TAG, "Select button pressed, toggling timer state");
                timer_control(!state.is_running);
                button_states[2] = false;
            }
        } else if (gpio_get_level(BTN_SELECT) == 1) {
            button_states[2] = true;
        }

        if (gpio_get_level(BTN_PREV) == 0 && button_states[3]) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY));
            if (gpio_get_level(BTN_PREV) == 0) {
                ESP_LOGI(TAG, "Select button pressed, toggling timer state");
                back_pressed_timer();
                button_states[3] = false;
            }
        } else if (gpio_get_level(BTN_PREV) == 1) {
            button_states[3] = true;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void format_time(uint32_t total_seconds, char *buffer, size_t buffer_size) {
    uint32_t minutes = total_seconds / 60;
    uint32_t seconds = total_seconds % 60;
    snprintf(buffer, buffer_size, "%02lu:%02lu", minutes, seconds);
}

static void timer_countdown_task(void *pvParameters) {
    ESP_LOGI(TAG, "Timer countdown task started");
    char time_str[8];  // Buffer for "MM:SS\0"
    
    while (1) {
        if (state.is_running && state.remaining_time > 0) {
            // Format current time
            format_time(state.remaining_time, time_str, sizeof(time_str));
            
            // Update the label text directly
            lv_label_set_text(ui.timer_label, time_str);
            lv_refr_now(NULL);
            // Decrement counter
            state.remaining_time--;
            
            if (state.remaining_time == 0) {
                ESP_LOGI(TAG, "Timer expired!");
                state.is_running = false;
                state.is_expired = true;
                lv_label_set_text(ui.timer_label, "00:00");
                turn_on_buzzer();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

lv_obj_t* chef_create_timer_screen(void) {

    init_buzzer();
    timer_init();

    ESP_LOGI(TAG, "Creating timer screen");
    timer_screen = lv_obj_create(NULL);
    extern lv_style_t screen_background;
    lv_obj_add_style(timer_screen, &screen_background, 0);

    ESP_LOGD(TAG, "Creating spinbox with range 0-%d seconds", MAX_TIME_SECONDS);
    ui.spinbox = lv_spinbox_create(timer_screen);
    lv_spinbox_set_range(ui.spinbox, 0, MAX_TIME_SECONDS);
    lv_spinbox_set_step(ui.spinbox, TIME_STEP_SECONDS);
    lv_obj_set_size(ui.spinbox, 120, 40);
    lv_obj_align(ui.spinbox, LV_ALIGN_TOP_MID, 0, 20);

    ESP_LOGD(TAG, "Creating timer display label");
    ui.timer_label = lv_label_create(timer_screen);
    lv_label_set_text(ui.timer_label, "00:00");
    lv_obj_set_style_text_font(ui.timer_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui.timer_label, lv_color_white(), 0);
    lv_obj_set_style_text_decor(ui.timer_label, LV_TEXT_DECOR_NONE, 0);
    lv_obj_set_style_text_opa(ui.timer_label, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(ui.timer_label, &lv_font_montserrat_12, 0); 
    lv_obj_align(ui.timer_label, LV_ALIGN_CENTER, 0, 0);

    ESP_LOGD(TAG, "Creating start/stop button");
    ui.start_btn = lv_btn_create(timer_screen);
    lv_obj_set_size(ui.start_btn, 120, 50);
    lv_obj_align(ui.start_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(ui.start_btn, lv_color_make(0, 255, 0), 0);

    lv_obj_t *btn_label = lv_label_create(ui.start_btn);
    lv_label_set_text(btn_label, "Start");
    lv_obj_center(btn_label);

    xTaskCreate(timer_countdown_task, "timer_task", 4096, NULL, 5, NULL);

    xTaskCreate(button_handler_task, "button_task", 4096, NULL, 5, &buttonhandle_timer);

    lv_scr_load(timer_screen);
    lv_refr_now(NULL);

    return timer_screen;
}