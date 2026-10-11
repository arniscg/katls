#include "../buttons.h"
#include "../common.h"
#include "../state.h"
#include "esp_bit_defs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "gui.h"
#include "lvgl.h"
#include "screen/screens.h"
#include "screen/wifi_screen.h"
#include "sdkconfig.h"
#include <unistd.h>

#if CONFIG_IDF_TARGET_LINUX
#include <SDL2/SDL.h>
#include "../wifi.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#else
#include "driver/gpio.h"
#include "driver/spi_master.h"
#endif

static uint32_t lv_tick_cb(void) { return (uint32_t)xTaskGetTickCount(); }

static const char *TAG = "GUI";

RingbufHandle_t gui_buf_handle;

#if CONFIG_IDF_TARGET_LINUX

// Desktop simulator: no real display/buttons, so LVGL renders into an SDL
// window and keyboard keys stand in for the physical buttons. The power
// button is handled by a dedicated task (mirroring the real board, where
// it's a separate button task) since calling enter_sleep() directly from
// this SDL callback - itself invoked from inside gui_task's own timer
// handling - would deadlock waiting on gui_task to process its own
// GUI_EVT_SLEEP message.
static SemaphoreHandle_t power_sem;

static void power_sim_task(void *arg) {
  while (1) {
    if (xSemaphoreTake(power_sem, portMAX_DELAY) == pdTRUE)
      enter_sleep();
  }
}

static int sdl_key_watch(void *userdata, SDL_Event *event) {
  if (event->type != SDL_KEYDOWN || event->key.repeat)
    return 0;

  uint8_t but;
  switch (event->key.keysym.sym) {
  case SDLK_1:
    but = BUTTON_F1;
    break;
  case SDLK_2:
    but = BUTTON_F2;
    break;
  case SDLK_3:
    but = BUTTON_F3;
    break;
  case SDLK_4:
    but = BUTTON_F4;
    break;
  case SDLK_RETURN:
    but = BUTTON_OK;
    break;
  case SDLK_ESCAPE:
    but = BUTTON_CANCEL;
    break;
  case SDLK_RIGHT:
    but = BUTTON_FORWARD;
    break;
  case SDLK_LEFT:
    but = BUTTON_BACK;
    break;
  case SDLK_p:
    but = BUTTON_POWER;
    break;
  default:
    return 0;
  }

  if (but == BUTTON_POWER) {
    xSemaphoreGive(power_sem);
    return 0;
  }

  char msg[2] = {GUI_EVT_BUTTON_PRESSED, but};
  if (xRingbufferSend(gui_buf_handle, msg, sizeof(msg), 0) != pdTRUE)
    ESP_LOGE(TAG, "failed to send simulated button press");
  return 0;
}

static lv_display_t *display_init() {
  ESP_LOGI(TAG, "simulator: opening SDL window");
  // The real panel is natively portrait and gets rotated 90° in software
  // (see the hardware branch below); lv_disp_set_rotation() on the SDL
  // driver reorients content without resizing the window to match, so
  // instead just open the window already in the rotated (landscape) shape.
  lv_display_t *display = lv_sdl_window_create(TFT_V_RES, TFT_H_RES);
  lv_sdl_window_set_title(display, "Katls simulator");
  lv_sdl_window_set_zoom(display, 3);
  lv_sdl_mouse_create();

  power_sem = xSemaphoreCreateBinary();
  xTaskCreate(power_sim_task, "PowerSim", 64 * 1024, NULL, 2, NULL);
  SDL_AddEventWatch(sdl_key_watch, NULL);

  // Buttons are ignored by the GUI until wifi.status == CONNECTED (see
  // screens_on_event). There's no real network in the simulator, so fake an
  // immediate connection here rather than relying on wifi_task to do it -
  // wifi_task runs at the same priority as this task and may not get
  // scheduled for a while once this task is busy with SDL/X11 calls.
  lock_state();
  state.wifi.status = WIFI_STATUS_CONNECTED;
  state.wifi.ntp_sync = true;
  unlock_state();

  // Same reasoning: journal.c sends onto wifi_buf_handle as soon as an entry
  // is created, which can happen (via simulated button presses) before
  // wifi_task has been scheduled to create it. Create it here instead so
  // it's always ready by the time the event loop below can act on anything.
  wifi_buf_handle = xRingbufferCreate(1024, RINGBUF_TYPE_NOSPLIT);
  assert(wifi_buf_handle != NULL);

  ESP_LOGI(TAG, "keys: 1-4=F1-F4  Enter=OK  Esc=Cancel  Right=Forward  "
                "Left=Back  P=Power/Sleep");
  return display;
}

#else

#define TFT_HOST SPI3_HOST

#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK 18
#define PIN_NUM_CS 5
#define PIN_NUM_DC 2
#define PIN_NUM_RST 15
#define PIN_NUM_LED 22

static spi_device_handle_t spi;

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

static lv_display_t *display_init() {
  ESP_LOGI(TAG, "enable display LED");
  const gpio_config_t conf = {
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = BIT64(PIN_NUM_LED),
  };
  gpio_config(&conf);
  gpio_set_level(PIN_NUM_LED, 1);

  spi_init();

  ESP_LOGI(TAG, "reset the display");
  gpio_set_level(PIN_NUM_RST, 0);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  gpio_set_level(PIN_NUM_RST, 1);
  vTaskDelay(100 / portTICK_PERIOD_MS);

  ESP_LOGI(TAG, "setup the display");
  lv_display_t *display = lv_st7735_create(
      TFT_H_RES, TFT_V_RES, LV_LCD_FLAG_NONE, tft_send_cmd, tft_send_color);
  lv_disp_set_rotation(display, LV_DISP_ROTATION_90);

  uint32_t buf_size =
      TFT_H_RES * TFT_V_RES *
      lv_color_format_get_size(lv_display_get_color_format(display));

  lv_color_t *buf = lv_malloc(buf_size);
  assert(buf != NULL);

  lv_display_set_buffers(display, buf, NULL, buf_size,
                         LV_DISPLAY_RENDER_MODE_FULL);
  return display;
}

#endif

void gui_task(void *) {
  ESP_LOGI(TAG, "started GUI task");

  gui_buf_handle = xRingbufferCreate(8192, RINGBUF_TYPE_NOSPLIT);
  assert(gui_buf_handle != NULL);

  lv_init();
  lv_tick_set_cb(lv_tick_cb);

  display_init();

  ESP_LOGI(TAG, "Started GUI task loop");

  screens_init();

  size_t item_size;
  char *item = NULL;
  while (1) {
    // consume all received events
    while (1) {
      item = (char *)xRingbufferReceive(gui_buf_handle, &item_size, 0);
      if (item == NULL)
        break;
      if (item[0] == GUI_EVT_SLEEP) {
        assert(item_size == 2);
#if !CONFIG_IDF_TARGET_LINUX
        bool enter = (bool)item[1];
        gpio_set_level(PIN_NUM_LED, enter ? 0 : 1);
#endif
        screens_go_home();
        assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
        state.sleepReady |= BIT(SLEEP_READY_GUI);
        assert(xSemaphoreGive(state_mutex) == pdTRUE);
      } else {
        screens_on_event(item[0], (uint8_t *)(item + 1), item_size - 1);
      }
      vRingbufferReturnItem(gui_buf_handle, item);
    }

    uint32_t time_till_next = lv_timer_handler();
    if (time_till_next == LV_NO_TIMER_READY)
      time_till_next = LV_DEF_REFR_PERIOD; /*handle LV_NO_TIMER_READY. Another
                                              option is to `sleep` for longer*/
    vTaskDelay(time_till_next / portTICK_PERIOD_MS);
  }
}
