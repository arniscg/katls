#include "journal_screen.h"
#include "../../journal.h"
#include <esp_log.h>

static const char *TAG = "journal_screen";

static void handle_event(BaseScreen *s, GUIEvent event, uint8_t *data,
                         unsigned size) {
  ESP_LOGI(TAG, "unhandled event %u", (unsigned)event);
}

static void update_list(JournalScreen *s) {
  ESP_LOGI(TAG, "update_list");
  assert(xSemaphoreTake(journal_mutex, portMAX_DELAY) == pdTRUE);

  BaseEntry *entries[JOURNAL_SIZE];
  unsigned n_entries = journal_get(entries, JOURNAL_SIZE);
  ESP_LOGI(TAG, "update_list: n entries: %u", n_entries);

  if (s->list)
    lv_obj_delete(s->list);
  s->list = lv_list_create(s->base.root);
  lv_obj_set_size(s->list, lv_pct(100), lv_pct(100));

  for (unsigned i = 0; i < n_entries; ++i) {
    ESP_LOGI(TAG, "update_list: %u", i);
    char str[256];
    entry_to_str(entries[i], str, sizeof(str));
    ESP_LOGI(TAG, "entry: %s", str);
    lv_list_add_text(s->list, str);
  }

  assert(xSemaphoreGive(journal_mutex) == pdTRUE);
}

static void on_load(BaseScreen *s) { update_list((JournalScreen *)s); }

void screen_journal_init(JournalScreen *s) {
  ESP_LOGI(TAG, "init");

  s->base.type = SCREEN_JOURNAL;
  s->base.on_event = &handle_event;
  s->base.on_load = &on_load;
  s->list = NULL;

  s->base.root = lv_obj_create(NULL);
  lv_obj_set_size(s->base.root, TFT_H_RES, TFT_V_RES);

  update_list(s);
}