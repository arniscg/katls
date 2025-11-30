#include "wifi_screen.h"
#include "../../state.h"
#include "../gui.h"
#include <esp_log.h>

const char *TAG = "wifi_screen";

static WifiScreen s;

static void convert_l8_to_rgb565(const uint8_t *src, uint16_t *dst, int w,
                                 int h) {
  int total = w * h;
  for (int i = 0; i < total; i++) {
    uint8_t g = src[i]; // grayscale 0–255

    // Convert grayscale → RGB565
    uint16_t r = (g >> 3) & 0x1F;
    uint16_t g5 = (g >> 2) & 0x3F;
    uint16_t b = (g >> 3) & 0x1F;

    dst[i] = (r << 11) | (g5 << 5) | b;
  }
}

static void fill_qr(Img *qr) {
  if (!s.qrcode) {
    s.qrcode = lv_image_create(s.cont);
  }

  lv_memzero(&s.dsc, sizeof(s.dsc));
  if (s.rgb)
    free(s.rgb);

  unsigned size = qr->w * qr->h * sizeof(uint16_t);
  s.rgb = (uint16_t *)malloc(size);
  convert_l8_to_rgb565(qr->data, s.rgb, qr->w, qr->h);

  unsigned scale = 2; // TODO: calculate dynamically

  s.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  s.dsc.header.cf = LV_COLOR_FORMAT_RGB565;
  s.dsc.header.w = qr->w;
  s.dsc.header.h = qr->h;
  s.dsc.data_size = size;
  s.dsc.data = (uint8_t *)s.rgb;

  lv_image_set_scale(s.qrcode, 256 * scale);
  lv_image_set_antialias(s.qrcode, false);
  lv_image_set_src(s.qrcode, &s.dsc);
}

static void update() {
  assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);

  if (s.lbl_state) {
    lv_obj_delete(s.lbl_state);
    s.lbl_state = NULL;
  }
  if (s.lbl_ssid) {
    lv_obj_delete(s.lbl_ssid);
    s.lbl_ssid = NULL;
  }
  if (s.lbl_ip) {
    lv_obj_delete(s.lbl_ip);
    s.lbl_ip = NULL;
  }
  if (s.qrcode) {
    lv_obj_delete(s.qrcode);
    s.qrcode = NULL;
  }

  switch (state.wifi.status) {
  case WIFI_STATUS_INIT:
    if (!s.lbl_state)
      s.lbl_state = lv_label_create(s.cont);
    lv_label_set_text(s.lbl_state, "Initializing");
    break;
  case WIFI_STATUS_WAITING_SCAN:
    assert(state.wifi.qrcode.data);
    fill_qr(&state.wifi.qrcode);
    break;
  case WIFI_STATUS_CONNECTING:
    if (!s.lbl_state)
      s.lbl_state = lv_label_create(s.cont);

    lv_label_set_text(s.lbl_state, "Connecting");
    if (!s.lbl_ssid)
      s.lbl_ssid = lv_label_create(s.cont);
    lv_label_set_text(s.lbl_ssid, (char *)state.wifi.conf.ssid);
    break;
  case WIFI_STATUS_CONNECTED:
    if (!s.lbl_state)
      s.lbl_state = lv_label_create(s.cont);
    lv_label_set_text(s.lbl_state, "Connected");
    if (!s.lbl_ssid)
      s.lbl_ssid = lv_label_create(s.cont);
    lv_label_set_text(s.lbl_ssid, (char *)state.wifi.conf.ssid);
    if (!s.lbl_ip)
      s.lbl_ip = lv_label_create(s.cont);
    char ipstr[16];
    lv_snprintf(ipstr, sizeof(ipstr), IPSTR, IP2STR(&state.wifi.ip));
    lv_label_set_text(s.lbl_ip, ipstr);
    break;
  default:
    break;
  }

  assert(xSemaphoreGive(state_mutex) == pdTRUE);
}

static void handle_event(GUIEvent event, void *data, unsigned size) {
  ESP_LOGI(TAG, "received event %u", (unsigned)event);
  if (event == GUI_EVT_WIFI_CHANGED) {
    update();
    return;
  }
  ESP_LOGI(TAG, "unhandled event %u", (unsigned)event);
}

BaseScreen *screen_wifi_init() {
  s.base.type = SCREEN_WIFI;
  s.base.on_event = &handle_event;

  s.base.root = lv_obj_create(NULL);
  lv_obj_set_size(s.base.root, TFT_H_RES, TFT_V_RES);

  s.cont = lv_obj_create(s.base.root);
  lv_obj_set_size(s.cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_border_width(s.cont, 0, 0);
  lv_obj_set_scrollbar_mode(s.cont, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(s.cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_align(s.cont, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_align(s.cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  s.lbl_state = NULL;
  s.lbl_ssid = NULL;
  s.lbl_ip = NULL;
  s.qrcode = NULL;
  s.rgb = NULL;
  lv_memset(&s.dsc, 0, sizeof(s.dsc));

  update();

  return (BaseScreen *)&s;
}