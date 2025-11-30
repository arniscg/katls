#pragma once

#include <lvgl.h>

typedef enum {
  GUI_EVT_BUTTON_PRESSED,
  GUI_EVT_WIFI_CHANGED,
} GUIEvent;

typedef void (*ScreenEventCb)(GUIEvent event, void *data, unsigned size);

typedef enum {
  SCREEN_WIFI,
} ScreenType;

typedef struct {
  ScreenType type;
  lv_obj_t *root;
  ScreenEventCb on_event;
} BaseScreen;