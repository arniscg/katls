#pragma once

#include "../gui.h"
#include <lvgl.h>

typedef struct BaseScreen BaseScreen;
typedef void (*ScreenEventCb)(BaseScreen *, GUIEvent, uint8_t *, unsigned);
typedef void (*OnScreenLoadCb)(BaseScreen *);

typedef enum {
  SCREEN_WIFI,
  SCREEN_INPUT,
  SCREEN_JOURNAL,
} ScreenType;

struct BaseScreen {
  ScreenType type;
  lv_obj_t *root;
  ScreenEventCb on_event;
  OnScreenLoadCb on_load;
  OnScreenLoadCb on_unload;
};