#ifndef WIFI_H
#define WIFI_H

#include "freertos/ringbuf.h"

typedef enum {
  WIFI_HTTP_SEND = 0,
  WIFI_HTTP_DEL = 1,
  WIFI_SLEEP = 2,
} WifiMsg;

extern RingbufHandle_t wifi_buf_handle;

void wifi_task(void *);

#endif