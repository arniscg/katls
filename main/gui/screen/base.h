#pragma once

#include "../gui.h"
#include <lvgl.h>

typedef struct BaseScreen BaseScreen;
typedef void (*ScreenEventCb)(BaseScreen *, GUIEvent, uint8_t *, unsigned);

typedef enum {
  SCREEN_WIFI,
  SCREEN_INPUT,
} ScreenType;

struct BaseScreen {
  ScreenType type;
  lv_obj_t *root;
  ScreenEventCb on_event;
};