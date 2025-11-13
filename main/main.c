/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "button_gpio.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

#define TFT_HOST SPI3_HOST
#define TFT_V_RES 160
#define TFT_H_RES 128

#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK 18
#define PIN_NUM_CS 5
#define PIN_NUM_DC 2
#define PIN_NUM_RST 15

#define BUTTON_1 32
#define BUTTON_2 33
#define BUTTON_3 25
#define BUTTON_4 26
#define BUTTON_5 27
#define BUTTON_6 14
#define BUTTON_7 12
#define BUTTON_8 13

static const char *TAG = "main";

extern void gui(lv_display_t *display);

spi_device_handle_t spi;

/* Send short command to the LCD. This function shall wait until the transaction
 * finishes. */
void tft_send_cmd(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size,
                  const uint8_t *param, size_t param_size) {
  ESP_LOGI(TAG, "tft_send_cmd size=%d", cmd_size);
  assert(cmd_size == 1);
  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = cmd_size * 8;
  t.tx_buffer = cmd;
  t.user = (void *)0;
  ret = spi_device_polling_transmit(spi, &t);
  assert(ret == ESP_OK);
  ESP_LOGI(TAG, "tft_send_cmd done");

  ESP_LOGI(TAG, "tft_send_cmd param size=%d", param_size);
  memset(&t, 0, sizeof(t));
  t.length = param_size * 8;
  t.tx_buffer = param;
  t.user = (void *)1;
  ret = spi_device_polling_transmit(spi, &t);
  assert(ret == ESP_OK);
  ESP_LOGI(TAG, "tft_send_cmd param done");
}

/* Send large array of pixel data to the LCD. If necessary, this function has to
 * do the byte-swapping. This function can do the transfer in the background. */
void tft_send_color(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size,
                    uint8_t *param, size_t param_size) {
  ESP_LOGI(TAG, "tft_send_color size= %d", param_size);
  tft_send_cmd(disp, cmd, cmd_size, param, param_size);
  ESP_LOGI(TAG, "tft_send_color done");
}

// This function is called (in irq context!) just before a transmission starts.
// It will set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
  int dc = (int)t->user;
  gpio_set_level(PIN_NUM_DC, dc);
  ESP_LOGI(TAG, "DC set to %d", dc);
}

void spi_init() {
  ESP_LOGI(TAG, "spi_init");
  esp_err_t ret;
  spi_bus_config_t buscfg = {.miso_io_num = -1,
                             .mosi_io_num = PIN_NUM_MOSI,
                             .sclk_io_num = PIN_NUM_CLK,
                             .quadwp_io_num = -1,
                             .quadhd_io_num = -1,
                             .max_transfer_sz = TFT_H_RES * TFT_V_RES * 2 + 8};
  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 8 * 1000 * 1000,
      .mode = 0,                  // SPI mode 0
      .spics_io_num = PIN_NUM_CS, // CS pin
      .queue_size = 1,
      .pre_cb = lcd_spi_pre_transfer_callback, // Specify pre-transfer callback
                                               // to handle D/C line
  };
  // Initialize the SPI bus
  ret = spi_bus_initialize(TFT_HOST, &buscfg, SPI_DMA_CH_AUTO);
  ESP_ERROR_CHECK(ret);
  // Attach the TFT to the SPI bus
  ret = spi_bus_add_device(TFT_HOST, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);
}

static void button_single_click_cb(void *arg, void *usr_data) {
  ESP_LOGI(TAG, "clicked button %d", (int)usr_data);
}

void button_init() {
  unsigned buttons[] = {BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4,
                        BUTTON_5, BUTTON_6, BUTTON_7, BUTTON_8};
  esp_err_t ret;
  unsigned i;
  for (i = 0; i < sizeof(buttons) / sizeof(unsigned); ++i) {
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = buttons[i],
        .active_level = 0,
    };
    button_handle_t gpio_btn = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &gpio_btn);
    ESP_ERROR_CHECK(ret);
    if (NULL == gpio_btn) {
      ESP_LOGE(TAG, "Button create failed");
    }
    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, NULL,
                           button_single_click_cb, (void *)(i + 1));
  }
}

void app_main(void) {
  button_init();

  // Initialize non-SPI GPIOs
  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = ((1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST));
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);

  spi_init();

  ESP_LOGI(TAG, "reset the display");
  gpio_set_level(PIN_NUM_RST, 0);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  gpio_set_level(PIN_NUM_RST, 1);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "reset the display done");

  ESP_LOGI(TAG, "lv init");
  lv_init();

  lv_tick_set_cb(xTaskGetTickCount);

  lv_display_t *display = lv_st7735_create(
      TFT_H_RES, TFT_V_RES, LV_LCD_FLAG_BGR, tft_send_cmd, tft_send_color);

  lv_disp_set_rotation(display, LV_DISP_ROTATION_90);

  uint32_t buf_size =
      TFT_H_RES * TFT_V_RES *
      lv_color_format_get_size(lv_display_get_color_format(display));

  ESP_LOGI(TAG, "pix size: %d",
           lv_color_format_get_size(lv_display_get_color_format(display)));

  lv_color_t *buf = lv_malloc(buf_size);
  assert(buf != NULL);

  lv_display_set_buffers(display, buf, NULL, buf_size,
                         LV_DISPLAY_RENDER_MODE_FULL);

  ESP_LOGI(TAG, "Display LVGL UI");

  gui(display);

  while (1) {
    lv_timer_handler();
    lv_tick_inc(100);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}