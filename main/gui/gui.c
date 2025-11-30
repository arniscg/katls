#include "gui.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "lvgl.h"
#include "screen/screen.h"
#include "screen/wifi_screen.h"
#include <unistd.h>

#define TFT_HOST SPI3_HOST

#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK 18
#define PIN_NUM_CS 5
#define PIN_NUM_DC 2
#define PIN_NUM_RST 15

static const char *TAG = "GUI";

RingbufHandle_t gui_buf_handle;
static spi_device_handle_t spi;

extern void gui();
extern void gui_button(uint8_t);
extern void gui_set_qrcode(uint8_t *, unsigned, unsigned);

static void tft_send_cmd(lv_display_t *disp, const uint8_t *cmd,
                         size_t cmd_size, const uint8_t *param,
                         size_t param_size) {
  assert(cmd_size == 1);
  esp_err_t ret;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = cmd_size * 8;
  t.tx_buffer = cmd;
  t.user = (void *)0;
  ret = spi_device_polling_transmit(spi, &t);
  assert(ret == ESP_OK);

  memset(&t, 0, sizeof(t));
  t.length = param_size * 8;
  t.tx_buffer = param;
  t.user = (void *)1;
  ret = spi_device_polling_transmit(spi, &t);
  assert(ret == ESP_OK);
}

static void tft_send_color(lv_display_t *disp, const uint8_t *cmd,
                           size_t cmd_size, uint8_t *param, size_t param_size) {
  // swap bytes in-place
  uint16_t *pixels = (uint16_t *)param;
  size_t count = param_size / 2;
  for (size_t i = 0; i < count; i++) {
    uint16_t p = pixels[i];
    pixels[i] = (p >> 8) | (p << 8);
  }

  tft_send_cmd(disp, cmd, cmd_size, param, param_size);
  lv_display_flush_ready(disp);
}

static void lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
  int dc = (int)t->user;
  gpio_set_level(PIN_NUM_DC, dc);
}

static void spi_init() {
  ESP_LOGI(TAG, "spi_init");

  // Initialize non-SPI GPIOs
  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = ((1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST));
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);

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
      .pre_cb = lcd_spi_pre_transfer_callback, // Specify pre-transfer
                                               // callback to handle D/C line
  };
  // Initialize the SPI bus
  ret = spi_bus_initialize(TFT_HOST, &buscfg, SPI_DMA_CH_AUTO);
  ESP_ERROR_CHECK(ret);
  // Attach the TFT to the SPI bus
  ret = spi_bus_add_device(TFT_HOST, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);
}

void gui_task(void *) {
  ESP_LOGI(TAG, "started GUI task");

  gui_buf_handle = xRingbufferCreate(8192, RINGBUF_TYPE_NOSPLIT);
  assert(gui_buf_handle != NULL);

  spi_init();

  ESP_LOGI(TAG, "reset the display");
  gpio_set_level(PIN_NUM_RST, 0);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  gpio_set_level(PIN_NUM_RST, 1);
  vTaskDelay(100 / portTICK_PERIOD_MS);

  lv_init();
  lv_tick_set_cb(xTaskGetTickCount);

  ESP_LOGI(TAG, "setup the display");
  lv_display_t *display = lv_st7735_create(
      TFT_H_RES, TFT_V_RES, LV_LCD_FLAG_NONE, tft_send_cmd, tft_send_color);
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

  ESP_LOGI(TAG, "Started GUI task loop");

  // gui();
  BaseScreen *s = screen_wifi_init();
  lv_scr_load(s->root);

  size_t item_size;
  char *item = NULL;
  while (1) {
    while (1) {
      item = (char *)xRingbufferReceive(gui_buf_handle, &item_size, 0);
      if (item == NULL) {
        // ESP_LOGI(TAG, "no item");
        break;
      }

      s->on_event(item[0], item + 1, item_size - 1);
      // if (msg == 0) {
      //   if (item_size != 2) {
      //     ESP_LOGI(TAG, "invalid button message");
      //     vRingbufferReturnItem(gui_buf_handle, item);
      //     continue;
      //   }
      //   uint8_t but = item[1];
      //   gui_button(but);
      // } else if (msg == 1) {
      //   ESP_LOGI(TAG, "received image");
      //   if (item_size < 4) {
      //     ESP_LOGI(TAG, "image too short");
      //     vRingbufferReturnItem(gui_buf_handle, item);
      //     continue;
      //   }
      //   unsigned w = (uint8_t)item[1];
      //   unsigned h = (uint8_t)item[2];
      //   assert(w * h == item_size - 3);
      //   ESP_LOGI(TAG, "image size: %u (%ux%u)", item_size - 3, w, h);
      //   gui_set_qrcode((uint8_t *)(item + 3), w, h);
      // } else {
      //   ESP_LOGI(TAG, "unhandled msg %d", msg);
      // }

      vRingbufferReturnItem(gui_buf_handle, item);
    }

    uint32_t time_till_next = lv_timer_handler();
    if (time_till_next == LV_NO_TIMER_READY)
      time_till_next = LV_DEF_REFR_PERIOD; /*handle LV_NO_TIMER_READY. Another
                                              option is to `sleep` for longer*/
    vTaskDelay(time_till_next / portTICK_PERIOD_MS);
  }
}