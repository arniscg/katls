#pragma once

#include "esp_wifi.h"
#include <inttypes.h>

// global application state used by whoever wants
// using mutex is mandatory when accessing state

typedef enum {
  WIFI_STATUS_INIT,
  WIFI_STATUS_WAITING_SCAN,
  WIFI_STATUS_CONNECTING,
  WIFI_STATUS_CONNECTED,
} WifiStatus;

typedef struct {
  WifiStatus status;
  uint8_t *qrcode;
  wifi_sta_config_t conf;
} WifiState;

typedef struct {
  WifiState wifi;
} State;

extern State state;
extern SemaphoreHandle_t state_mutex;

void state_init();