#ifndef GUI_H
#define GUI_H

#include "freertos/ringbuf.h"

extern RingbufHandle_t gui_buf_handle;

void gui_task(void *);

#endif /* GUI_H */