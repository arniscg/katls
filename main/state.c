#include "state.h"
#include <string.h>

State state;
SemaphoreHandle_t state_mutex;

void state_init() {
  state_mutex = xSemaphoreCreateMutex();
  state.wifi.status = WIFI_STATUS_INIT;
  memset(&state.wifi.qrcode, 0, sizeof(state.wifi.qrcode));
  memset(&state.wifi.conf, 0, sizeof(state.wifi.conf));
  state.wifi.ip.addr = 0;
  state.wifi.ntp_sync = false;
  state.id = 0;
}