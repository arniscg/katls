#pragma once

#include "esp_wifi.h"
#include <inttypes.h>

// global application state used by whoever wants
// using mutex is mandatory when accessing state

typedef enum {
  WIFI_STATUS_INIT,
  WIFI_STATUS_DPP_WAITING,
  WIFI_STATUS_CONNECTING,
  WIFI_STATUS_CONNECTED,
  WIFI_STATUS_FAILED,
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
  bool ntp_sync;
} WifiState;

typedef struct {
  WifiState wifi;
  unsigned id;
} State;

extern State state;
extern SemaphoreHandle_t state_mutex;

void state_init();