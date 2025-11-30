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
  uint8_t *data;
  unsigned w;
  unsigned h;
} Img;

typedef struct {
  WifiStatus status;
  Img qrcode;
  wifi_sta_config_t conf;
  esp_ip4_addr_t ip;
} WifiState;

typedef struct {
  WifiState wifi;
} State;

extern State state;
extern SemaphoreHandle_t state_mutex;

void state_init();