#include <stdio.h>
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "chef_button.h"
#include "lvgl.h"
#include "driver/ledc.h"


void setup_buttons() {
    gpio_pad_select_gpio(BTN_NEXT);
    gpio_set_direction(BTN_NEXT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_NEXT, GPIO_PULLUP_ONLY);

    gpio_pad_select_gpio(BTN_PREV);
    gpio_set_direction(BTN_PREV, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_PREV, GPIO_PULLUP_ONLY);

    gpio_pad_select_gpio(BTN_UP);
    gpio_set_direction(BTN_UP, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_UP, GPIO_PULLUP_ONLY);

    gpio_pad_select_gpio(BTN_DOWN);
    gpio_set_direction(BTN_DOWN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_DOWN, GPIO_PULLUP_ONLY);

    gpio_pad_select_gpio(BTN_SELECT);
    gpio_set_direction(BTN_SELECT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_SELECT, GPIO_PULLUP_ONLY);
}

void init_buzzer() {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0, // Start with buzzer off
        .hpoint = 0
    };
    ledc_channel_config(&channel_conf);
}

void turn_on_buzzer() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4095);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void turn_off_buzzer() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}