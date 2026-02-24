#pragma once

#include "esp_wifi.h"
#include <inttypes.h>
#include <time.h>

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

typedef enum {
  SLEEP_READY_WIFI = 0,
  SLEEP_READY_GUI = 1,
} SleepReadyBit;

typedef struct {
  WifiState wifi;
  unsigned id;
  unsigned sleepReady;
  time_t last_touched;
} State;

extern State state;
extern SemaphoreHandle_t state_mutex;

void state_init();