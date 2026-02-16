#ifndef GUI_H
#define GUI_H

#include "freertos/ringbuf.h"

#define TFT_V_RES 160
#define TFT_H_RES 128

typedef enum {
  GUI_EVT_BUTTON_PRESSED,
  GUI_EVT_WIFI_CHANGED,
  GUI_EVT_JOURNAL_CHANGED,
} GUIEvent;

extern RingbufHandle_t gui_buf_handle;

void gui_task(void *);

#endif /* GUI_H */