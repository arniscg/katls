#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <inttypes.h>
#include <time.h>

#if CONFIG_IDF_TARGET_LINUX
// Desktop simulator build: no real esp_wifi/esp_netif stack. These mirror
// just the fields the GUI code (wifi_screen.c) actually reads.
typedef struct {
  uint8_t ssid[32];
  uint8_t password[64];
} wifi_sta_config_t;
typedef struct {
  uint32_t addr;
} esp_ip4_addr_t;
#define IPSTR "%d.%d.%d.%d"
#define IP2STR(ipaddr)                                                       \
  ((uint8_t *)&(ipaddr)->addr)[0], ((uint8_t *)&(ipaddr)->addr)[1],           \
      ((uint8_t *)&(ipaddr)->addr)[2], ((uint8_t *)&(ipaddr)->addr)[3]
#else
#include "esp_wifi.h"
#endif

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