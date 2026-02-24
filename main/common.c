#include "common.h"
#include "gui/gui.h"
#include "state.h"
#include "wifi.h"
#include <esp_log.h>
#include <esp_sleep.h>

static const char *TAG = "COMMON";

size_t timeStr(time_t *now, char *buf, size_t len, bool sh) {
  struct tm timeinfo;
  localtime_r(now, &timeinfo);
  if (sh)
    return strftime(buf, len, "%d.%m %H:%M", &timeinfo);
  else
    return strftime(buf, len, "%d.%m.%Y %H:%M", &timeinfo);
}

static unsigned get_sleep_state() {
  unsigned s = 0;
  assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
  s = state.sleepReady;
  assert(xSemaphoreGive(state_mutex) == pdTRUE);
  return s;
}

void enter_sleep() {
  lock_state();
  state.sleepReady = 0;
  unlock_state();

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

void lock_state() {
  assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
}

void unlock_state() { assert(xSemaphoreGive(state_mutex) == pdTRUE); }