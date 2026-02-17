#include "journal_screen.h"
#include "../../buttons.h"
#include "../../journal.h"
#include <esp_log.h>

static const char *TAG = "journal_screen";

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

  for (int i = n_entries - 1; i >= 0; --i) {
    ESP_LOGI(TAG, "update_list: %u", i);
    char str[256];
    entry_to_str(entries[i], str, sizeof(str));
    ESP_LOGI(TAG, "entry: %s", str);
    lv_obj_t *text = lv_list_add_text(s->list, str);

    lv_color_t color;
    switch (entries[i]->state) {
    case ENTRY_STATE_INIT:
      color = lv_color_hex(0x808080);
      break;
    case ENTRY_STATE_STORING:
      color = lv_color_hex(0xf5ff87);
      break;
    case ENTRY_STATE_STORE_FAILED:
      color = lv_color_hex(0xcf5555);
      break;
    case ENTRY_STATE_STORED:
      color = lv_color_hex(0xbcffad);
      break;
    default:
      break;
    }
    lv_obj_set_style_bg_color(text, color, LV_PART_MAIN);
  }

  assert(xSemaphoreGive(journal_mutex) == pdTRUE);
}

static void on_button(JournalScreen *s, uint8_t button) {
  switch (button) {
  case BUTTON_OK:
    assert(xSemaphoreTake(journal_mutex, portMAX_DELAY) == pdTRUE);

    BaseEntry *entries[JOURNAL_SIZE] = {0};
    unsigned n = journal_get(entries, JOURNAL_SIZE);

    for (unsigned i = 0; i < n; ++i) {
      BaseEntry *e = entries[i];
      if (e->state != ENTRY_STATE_STORE_FAILED)
        continue;
      e->storetry = 0;
      entry_state_update(e, ENTRY_STATE_STORING);
    }

    assert(xSemaphoreGive(journal_mutex) == pdTRUE);
    break;
  case BUTTON_CANCEL:
    assert(xSemaphoreTake(journal_mutex, portMAX_DELAY) == pdTRUE);
    journal_pop();
    assert(xSemaphoreGive(journal_mutex) == pdTRUE);
    break;
  default:
    break;
  }
}

static void handle_event(BaseScreen *s, GUIEvent event, uint8_t *data,
                         unsigned size) {
  if (event == GUI_EVT_JOURNAL_CHANGED) {
    update_list((JournalScreen *)s);
    return;
  } else if (event == GUI_EVT_BUTTON_PRESSED) {
    assert(size == 1);
    uint8_t b = data[0];
    on_button((JournalScreen *)s, b);
  }

  ESP_LOGI(TAG, "unhandled event %u", (unsigned)event);
}

static void on_load(BaseScreen *s) { update_list((JournalScreen *)s); }
static void on_unload(BaseScreen *s) {
  JournalScreen *scr = (JournalScreen *)s;
  if (scr->list) {
    lv_obj_delete(scr->list);
    scr->list = NULL;
  }
}

void screen_journal_init(JournalScreen *s) {
  ESP_LOGI(TAG, "init");

  s->base.type = SCREEN_JOURNAL;
  s->base.on_event = &handle_event;
  s->base.on_load = &on_load;
  s->base.on_unload = &on_unload;
  s->list = NULL;

  s->base.root = lv_obj_create(NULL);
  lv_obj_set_size(s->base.root, TFT_H_RES, TFT_V_RES);

  update_list(s);
}