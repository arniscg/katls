#ifndef GUI_H
#define GUI_H

#include "freertos/ringbuf.h"

#define TFT_V_RES 160
#define TFT_H_RES 128

typedef enum {
  GUI_EVT_BUTTON_PRESSED = 0,
  GUI_EVT_WIFI_CHANGED = 1,
  GUI_EVT_JOURNAL_CHANGED = 2,
  GUI_EVT_SLEEP = 3,
} GUIEvent;

extern RingbufHandle_t gui_buf_handle;

void gui_task(void *);

#endif /* GUI_H */