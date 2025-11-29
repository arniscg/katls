#include "state.h"
#include <string.h>

State state;
SemaphoreHandle_t state_mutex;

void state_init() {
  state_mutex = xSemaphoreCreateMutex();
  state.wifi.status = WIFI_STATUS_INIT;
  state.wifi.qrcode = NULL;
  memset(&state, 0, sizeof(state.wifi.conf));
}