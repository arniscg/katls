#include "input_screen.h"
#include "../../buttons.h"
#include "../../journal.h"
#include "../../wifi.h"
#include "screens.h"
#include <esp_log.h>

static const char *TAG = "input_screen";

static void update(InputScreen *s) {
  switch (s->state) {
  case INPUT_TEMP: {
    char str[4];
    lv_snprintf(str, 4, "%dC", s->temp);
    lv_label_set_text(s->lbl_prompt, "set temperature:");
    lv_label_set_text(s->lbl_value, str);
    break;
  }
  case INPUT_BAGS: {
    char str[3];
    lv_snprintf(str, 3, "%d", s->bags);
    lv_label_set_text(s->lbl_prompt, "added bags:");
    lv_label_set_text(s->lbl_value, str);
    break;
  }
  case INPUT_CLEAN:
    lv_label_set_text(s->lbl_prompt, "cleaning");
    lv_label_set_text(s->lbl_value, "done");
    break;
  case INPUT_STOP:
    lv_label_set_text(s->lbl_prompt, "stop");
    lv_label_set_text(s->lbl_value, "pechka");
    break;
  case INPUT_JOURNAL_ADD:
    lv_obj_set_style_bg_color(s->cont, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_label_set_text(s->lbl_prompt, "adding to");
    lv_label_set_text(s->lbl_value, "journal ...");
    break;
  default:
    break;
  }
}

static BaseEntry *state_to_journal_entry(InputScreen *s) {
  BaseEntry *be = NULL;

  switch (s->state) {
  case INPUT_TEMP: {
    TempEntry *e = (TempEntry *)malloc(sizeof(TempEntry));
    e->b.type = ENTRY_TYPE_TEMP;
    e->temp = s->temp;
    be = (BaseEntry *)e;
    break;
  }
  case INPUT_BAGS: {
    BagsEntry *e = (BagsEntry *)malloc(sizeof(BagsEntry));
    e->b.type = ENTRY_TYPE_BAGS;
    e->count = s->bags;
    be = (BaseEntry *)e;
    break;
  }
  case INPUT_CLEAN: {
    CleanEntry *e = (CleanEntry *)malloc(sizeof(CleanEntry));
    e->b.type = ENTRY_TYPE_CLEAN;
    be = (BaseEntry *)e;
    break;
  }
  case INPUT_STOP: {
    StopEntry *e = (StopEntry *)malloc(sizeof(StopEntry));
    e->b.type = ENTRY_TYPE_STOP;
    be = (BaseEntry *)e;
    break;
  }
  default:
    break;
  }

  entry_init(be);
  return be;
}

static InputScreen *x;
static void on_ok_done(TimerHandle_t) {
  ESP_LOGI(TAG, "on_ok_done");
  lv_obj_set_style_bg_color(x->cont, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  screens_return();
}

static void on_button(InputScreen *s, uint8_t but) {
  ESP_LOGI(TAG, "on_button: %u", but);

  if (but == BUTTON_F1)
    s->state = INPUT_TEMP;
  else if (but == BUTTON_F2)
    s->state = INPUT_BAGS;
  else if (but == BUTTON_F3)
    s->state = INPUT_CLEAN;
  else if (but == BUTTON_F4)
    s->state = INPUT_STOP;

  switch (but) {
  case BUTTON_F1:
    /* fallthrough */
  case BUTTON_F2:
    /* fallthrough */
  case BUTTON_F3:
    /* fallthrough */
  case BUTTON_F4:
    s->temp = 50;
    s->bags = 1;
    s->timer = NULL;
    break;
  case BUTTON_FORWARD:
    if (s->temp < 70)
      s->temp += 5;
    if (s->bags < 12)
      s->bags++;
    break;
  case BUTTON_BACK:
    if (s->temp > 40)
      s->temp -= 5;
    if (s->bags > 1)
      s->bags--;
    break;
  case BUTTON_OK: {
    BaseEntry *e = state_to_journal_entry(s);
    assert(xSemaphoreTake(journal_mutex, portMAX_DELAY) == pdTRUE);
    assert(journal_add(e));
    assert(xSemaphoreGive(journal_mutex) == pdTRUE);
    s->state = INPUT_JOURNAL_ADD;
    x = s;
    TimerCallbackFunction_t cb = &on_ok_done;
    s->timer = xTimerCreate("ok_cancel_timer", pdMS_TO_TICKS(1000), pdFALSE,
                            (void *)0, cb);
    if (s->timer == NULL) {
      ESP_LOGE(TAG, "failed to create timer");
      on_ok_done(s->timer);
      return;
    }
    assert(xTimerStart(s->timer, 0) == pdTRUE);
    break;
  }
  default:
    break;
  }

  update(s);
}

static void handle_event(BaseScreen *s, GUIEvent event, uint8_t *data,
                         unsigned size) {
  ESP_LOGI(TAG, "event: %u", (unsigned)event);

  switch (event) {
  case GUI_EVT_BUTTON_PRESSED:
    assert(size == 1);
    uint8_t b = data[0];
    on_button((InputScreen *)s, b);
    break;
  default:
    break;
  }
}

void screen_input_init(InputScreen *s) {
  ESP_LOGI(TAG, "init");
  s->base.root = lv_obj_create(NULL);
  s->base.type = SCREEN_INPUT;
  s->base.on_event = &handle_event;

  s->cont = lv_obj_create(s->base.root);
  lv_obj_set_size(s->cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_border_width(s->cont, 0, 0);
  lv_obj_set_style_radius(s->cont, 0, LV_PART_MAIN);
  lv_obj_set_scrollbar_mode(s->cont, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(s->cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_align(s->cont, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_align(s->cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  s->lbl_prompt = lv_label_create(s->cont);
  s->lbl_value = lv_label_create(s->cont);
}