#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_conf.h"

// Pin definitions
#define PIN_MOSI 18 
#define PIN_SCLK 5 
#define PIN_CS   16
#define PIN_DC   17 
#define PIN_RST  21 
#define PIN_BL   4  // Backlight pin

// Display dimensions
#define ST7735_WIDTH  128
#define ST7735_HEIGHT 160

// ST7735 commands
#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT  0x11
#define ST7735_NORON   0x13
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_MADCTL  0x36
#define ST7735_COLMOD  0x3A


static const char *TAG = "ST7735";
static spi_device_handle_t spi;

// Initialize GPIO
static void gpio_init(void) {
    gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BL, GPIO_MODE_OUTPUT);
    
    gpio_set_level(PIN_BL, 1);
    gpio_set_level(PIN_DC, 0);
}

// Hardware reset
static void lcd_reset(void) {
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

// Send command to display
void tft_cmd(const uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_data[0] = cmd;
    t.flags = SPI_TRANS_USE_TXDATA;
    gpio_set_level(PIN_DC, 0);  // Command mode
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
}

void tft_data(const uint8_t *data, int len) {
    if (len == 0) return;

    // Split data into smaller chunks (e.g., 1024 bytes)
    const int chunk_size = 1024;
    int remaining = len;
    const uint8_t *data_ptr = data;

    while (remaining > 0) {
        int current_len = (remaining > chunk_size) ? chunk_size : remaining;
        
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = current_len * 8;
        t.tx_buffer = data_ptr;
        gpio_set_level(PIN_DC, 1);
        esp_err_t ret = spi_device_polling_transmit(spi, &t);
        assert(ret == ESP_OK);

        data_ptr += current_len;
        remaining -= current_len;
    }
}

// Initialize SPI
static void spi_init() {
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_device_interface_config_t devcfg={
        .mode=3,
        .cs_ena_pretrans=2,                    
        .clock_speed_hz=40*1000*1000,          
        .spics_io_num=PIN_CS,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size=7,
        .pre_cb=NULL,
        .post_cb=NULL,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));
}

// Set drawing window
void st7735_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    // Column address set
    tft_cmd(ST7735_CASET);
    data[0] = (x0 >> 8) & 0xFF;  // MSB of x0
    data[1] = x0 & 0xFF;         // LSB of x0
    data[2] = (x1 >> 8) & 0xFF;  // MSB of x1
    data[3] = x1 & 0xFF;         // LSB of x1
    tft_data(data, 4);

    // Row address set
    tft_cmd(ST7735_RASET);
    data[0] = (y0 >> 8) & 0xFF;  // MSB of y0
    data[1] = y0 & 0xFF;         // LSB of y0
    data[2] = (y1 >> 8) & 0xFF;  // MSB of y1
    data[3] = y1 & 0xFF;         // LSB of y1
    tft_data(data, 4);

    // Write to RAM
    tft_cmd(ST7735_RAMWR);
}

void st7735_init() {

    gpio_init();
    spi_init();
    

    lcd_reset();
    
    tft_cmd(ST7735_SWRESET);    // Software reset
    vTaskDelay(pdMS_TO_TICKS(150));
    
    tft_cmd(ST7735_SLPOUT);     // Out of sleep mode
    vTaskDelay(pdMS_TO_TICKS(500));
    
    tft_cmd(ST7735_COLMOD);     // Color mode
    {
        uint8_t data = 0x05;     // 16-bit color
        tft_data(&data, 1);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    
    tft_cmd(ST7735_MADCTL);     // Memory access control
    {
        uint8_t data = 0x08;     // RGB color order
        tft_data(&data, 1);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Set full display window
    st7735_set_window(0, 0, ST7735_WIDTH - 1, ST7735_HEIGHT - 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    tft_cmd(ST7735_NORON);      // Normal display on
    vTaskDelay(pdMS_TO_TICKS(10));
    
    tft_cmd(ST7735_DISPON);     // Display on
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Display initialized");
}

void st7735_clear_display() {
    // Set window to entire display
    st7735_set_window(0, 0, ST7735_WIDTH - 1, ST7735_HEIGHT - 1);
    
    // Calculate buffer size for one line
    uint16_t line_buffer[ST7735_WIDTH];
    // Fill buffer with zeros (black pixels)
    for(int i = 0; i < ST7735_WIDTH; i++) {
        line_buffer[i] = 0x0000;  // Black in RGB565 format
    }
    
    // Write black pixels line by line
    for(int y = 0; y < ST7735_HEIGHT; y++) {
        tft_data((uint8_t*)line_buffer, ST7735_WIDTH * 2);  // *2 because each pixel is 2 bytes
    }
}


static void display_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    size_t width = area->x2 - area->x1 + 1;
    size_t height = area->y2 - area->y1 + 1;

    st7735_set_window(area->x1, area->y1, area->x2, area->y2);

    tft_data(px_map, width * height * 2);

    lv_display_flush_ready(disp);
}

void lvgl_init_all(){

    st7735_init();
    st7735_clear_display();

    ESP_LOGI(TAG, "Starting LVGL");

    lv_init();

    static lv_color_t buf1[ST7735_WIDTH * ST7735_HEIGHT] = {0};  // Declare a buffer for 10 lines
    
    // Create a display
    lv_display_t * display = lv_display_create(ST7735_WIDTH, ST7735_HEIGHT);
    lv_display_set_buffers(display, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, display_flush_cb);
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_0);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    lv_disp_t* disp = lv_disp_get_default();
    if (!disp) {
        ESP_LOGE(TAG, "Display driver not initialized");
    }
}