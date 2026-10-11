#include "common.h"
#include "gui/gui.h"
#include "state.h"
#include "wifi.h"
#include <esp_bit_defs.h>
#include <esp_err.h>
#include <esp_log.h>
#if !CONFIG_IDF_TARGET_LINUX
#include <esp_sleep.h>
#endif

static const char *TAG = "COMMON";

static bool sleep_in_progress = false;

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
  if (sleep_in_progress) {
    unlock_state();
    ESP_LOGW(TAG, "enter_sleep: already sleeping/entering, ignoring");
    return;
  }
  sleep_in_progress = true;
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
#if CONFIG_IDF_TARGET_LINUX
  ESP_LOGI(TAG, "simulator: skipping real light sleep, waking immediately");
#else
  esp_err_t err = esp_light_sleep_start();
  if (err != ESP_OK)
    ESP_LOGE(TAG, "esp_light_sleep_start failed: %s", esp_err_to_name(err));
  else
    ESP_LOGI(TAG, "wake-up from light sleep");
#endif

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
  sleep_in_progress = false;
  assert(xSemaphoreGive(state_mutex) == pdTRUE);
}

void lock_state() {
  assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
}

void unlock_state() { assert(xSemaphoreGive(state_mutex) == pdTRUE); }