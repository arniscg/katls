#pragma once

#include "base.h"

typedef struct {
  BaseScreen base;
  lv_obj_t *cont;
  lv_obj_t *lbl_state;
  lv_obj_t *lbl_ssid;
  lv_obj_t *lbl_ip;
  lv_obj_t *lbl_time;

  lv_obj_t *qrcode;
  lv_image_dsc_t dsc;
  uint16_t *rgb;

  lv_timer_t *timer;
} WifiScreen;

void screen_wifi_init(WifiScreen *s);