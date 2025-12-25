#ifndef WIFI_H
#define WIFI_H

#include "freertos/ringbuf.h"

typedef enum {
  WIFI_HTTP_SEND = 0,
} WifiMsg;

extern RingbufHandle_t wifi_buf_handle;

void wifi_task(void *);

#endif