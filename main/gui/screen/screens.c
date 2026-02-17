#include "screens.h"
#include "../../buttons.h"
#include "../../state.h"
#include "base.h"
#include "input_screen.h"
#include "journal_screen.h"
#include "wifi_screen.h"
#include <esp_log.h>

static const char *TAG = "screens";

static BaseScreen *curr_screen = NULL;
static BaseScreen *prev_screen = NULL;

static WifiScreen wifi_screen;
static InputScreen input_screen;
static JournalScreen journal_screen;

static void change_screen(BaseScreen *scr) {
  if (curr_screen == scr)
    return;
  prev_screen = curr_screen;
  curr_screen = scr;

  if (prev_screen && prev_screen->on_unload)
    prev_screen->on_unload(prev_screen);

  lv_scr_load(curr_screen->root);

  if (curr_screen->on_load)
    curr_screen->on_load(curr_screen);
}

void screens_init() {
  ESP_LOGI(TAG, "init");
  screen_wifi_init(&wifi_screen);
  screen_input_init(&input_screen);
  screen_journal_init(&journal_screen);

  change_screen((BaseScreen *)&wifi_screen);
}

void screens_on_event(GUIEvent ev, uint8_t *data, unsigned size) {
  ESP_LOGI(TAG, "event: %u", (unsigned)ev);

  switch (ev) {
  case GUI_EVT_BUTTON_PRESSED:
    assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
    if (state.wifi.status != WIFI_STATUS_CONNECTED) {
      ESP_LOGI(TAG, "ignore event, WiFi not connected");
      if (curr_screen->type != SCREEN_WIFI)
        change_screen((BaseScreen *)&wifi_screen);
      assert(xSemaphoreGive(state_mutex) == pdTRUE);
      return;
    }
    assert(xSemaphoreGive(state_mutex) == pdTRUE);

    uint8_t b = data[0];

    if (curr_screen->type != SCREEN_INPUT && b == BUTTON_FORWARD) {
      if (curr_screen->type == SCREEN_WIFI)
        change_screen((BaseScreen *)&journal_screen);
      else if (curr_screen->type == SCREEN_JOURNAL)
        change_screen((BaseScreen *)&wifi_screen);
      return;
    } else if ((b == BUTTON_F1 || b == BUTTON_F2 || b == BUTTON_F3 ||
                b == BUTTON_F4) &&
               (!curr_screen || curr_screen->type != SCREEN_INPUT)) {
      change_screen((BaseScreen *)&input_screen);
    }
    break;
  default:
    break;
  }

  if (curr_screen)
    curr_screen->on_event(curr_screen, ev, data, size);
}

void screens_return() {
  ESP_LOGI(TAG, "return");
  if (prev_screen)
    change_screen(prev_screen);
}