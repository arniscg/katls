#include "buttons.h"
#include "button_gpio.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/ringbuf.h"
#include "gui/gui.h"
#include "iot_button.h"
#include "state.h"
#include "wifi.h"
#include <esp_check.h>
#include <esp_sleep.h>

static const char *TAG = "BUTTONS";

static unsigned get_sleep_state() {
  unsigned s = 0;
  assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
  s = state.sleepReady;
  assert(xSemaphoreGive(state_mutex) == pdTRUE);
  return s;
}

static void enter_sleep() {
  assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
  state.sleepReady = 0;
  assert(xSemaphoreGive(state_mutex) == pdTRUE);

  ESP_LOGI(TAG, "entering sleep mode");
  char msg[2] = {GUI_EVT_SLEEP, 1};
  if (xRingbufferSend(gui_buf_handle, msg, sizeof(msg), 0) != pdTRUE)
    ESP_LOGE(TAG, "failed to send gui sleep enter");
  msg[0] = WIFI_SLEEP;
  if (xRingbufferSend(wifi_buf_handle, msg, sizeof(msg), 0) != pdTRUE)
    ESP_LOGE(TAG, "failed to send wifi sleep enter");

  unsigned sleepReady = BIT(SLEEP_READY_GUI) | BIT(SLEEP_READY_WIFI);
  unsigned s = get_sleep_state();
  while (s != sleepReady) {
    ESP_LOGI(TAG, "sleep no ready yet, state: %u, expected: %u", s, sleepReady);
    vTaskDelay(pdMS_TO_TICKS(500));
    s = get_sleep_state();
  }
  ESP_ERROR_CHECK(esp_light_sleep_start());
  ESP_LOGI(TAG, "wake-up from light sleep");

  ESP_LOGI(TAG, "exiting sleep mode");
  msg[1] = 0;
  msg[0] = GUI_EVT_SLEEP;
  if (xRingbufferSend(gui_buf_handle, msg, sizeof(msg), 0) != pdTRUE)
    ESP_LOGE(TAG, "failed to send gui sleep exit");
  msg[0] = WIFI_SLEEP;
  if (xRingbufferSend(wifi_buf_handle, msg, sizeof(msg), 0) != pdTRUE)
    ESP_LOGE(TAG, "failed to send wifi sleep exit");

  assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
  state.sleepReady = 0;
  assert(xSemaphoreGive(state_mutex) == pdTRUE);
}

static void button_single_click_cb(void *arg, void *usr_data) {
  uint8_t but_n = (uint8_t)(int)usr_data;

  if (but_n == BUTTON_POWER) {
    enter_sleep();
    return;
  }

  char msg[2] = {GUI_EVT_BUTTON_PRESSED, but_n};

  if (xRingbufferSend(gui_buf_handle, msg, sizeof(msg), 0) != pdTRUE)
    ESP_LOGE(TAG, "failed to send button press");
}

void buttons_init() {
  unsigned buttons[] = {BUTTON_F1,      BUTTON_F2,   BUTTON_F3,
                        BUTTON_F4,      BUTTON_OK,   BUTTON_CANCEL,
                        BUTTON_FORWARD, BUTTON_BACK, BUTTON_POWER};

  unsigned i;
  for (i = 0; i < sizeof(buttons) / sizeof(unsigned); ++i) {
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = buttons[i], .active_level = 0, .enable_power_save = true};

    button_handle_t gpio_btn = NULL;
    ESP_ERROR_CHECK(
        iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &gpio_btn));
    if (gpio_btn == NULL)
      ESP_LOGE(TAG, "Button create failed");

    ESP_ERROR_CHECK(iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, NULL,
                                           button_single_click_cb,
                                           (void *)buttons[i]));
  }
}