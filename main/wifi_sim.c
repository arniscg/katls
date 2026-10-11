#include "wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "journal.h"
#include <string.h>

// Desktop simulator build: no real network stack. Journal entries are
// "stored" immediately (always succeeds) so the GUI's journal screen can
// still be exercised end-to-end without a real backend. (Faking a
// "connected" wifi state happens in gui.c, not here - see the comment
// there for why.)

RingbufHandle_t wifi_buf_handle;

static const char *TAG = "wifi-sim";

static void handle_msg(char *data, size_t size) {
  if (size < 1)
    return;

  WifiMsg msg = (WifiMsg)data[0];

  if (msg == WIFI_HTTP_SEND) {
    if (size < 6)
      return;
    unsigned id = 0;
    memcpy(&id, data + 1, sizeof(id));
    ESP_LOGI(TAG, "simulating store success for entry %u", id);

    assert(xSemaphoreTake(journal_mutex, portMAX_DELAY) == pdTRUE);
    BaseEntry *e = journal_find_entry(id);
    if (e)
      entry_state_update(e, ENTRY_STATE_STORED);
    assert(xSemaphoreGive(journal_mutex) == pdTRUE);
  } else if (msg == WIFI_HTTP_DEL) {
    ESP_LOGI(TAG, "simulating delete (no-op)");
  } else if (msg == WIFI_SLEEP) {
    ESP_LOGI(TAG, "sleep message received (no real network to stop/start)");
  }
}

void wifi_task(void *) {
  // wifi_buf_handle is created eagerly in gui.c's display_init(), not here -
  // see the comment there for why.
  while (wifi_buf_handle == NULL)
    vTaskDelay(pdMS_TO_TICKS(10));

  ESP_LOGI(TAG, "simulator wifi task running - entries are always \"stored\" immediately");

  size_t item_size;
  char *item = NULL;
  while (1) {
    item = (char *)xRingbufferReceive(wifi_buf_handle, &item_size, pdMS_TO_TICKS(1000));
    if (item == NULL)
      continue;

    handle_msg(item, item_size);
    vRingbufferReturnItem(wifi_buf_handle, item);
  }
}
