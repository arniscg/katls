#include "buttons.h"
#include "button_gpio.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/ringbuf.h"
#include "gui/gui.h"
#include "iot_button.h"

static const char *TAG = "BUTTONS";

static void button_single_click_cb(void *arg, void *usr_data) {
  uint8_t but_n = (uint8_t)(int)usr_data;

  char msg[2] = {0, but_n};

  UBaseType_t res = xRingbufferSend(gui_buf_handle, msg, sizeof(msg), 0);
  if (res != pdTRUE) {
    printf("Failed to send button press\n");
  }
}

void buttons_init() {
  unsigned buttons[] = {BUTTON_F1, BUTTON_F2,     BUTTON_F3,      BUTTON_F4,
                        BUTTON_OK, BUTTON_CANCEL, BUTTON_FORWARD, BUTTON_BACK};

  esp_err_t ret;
  unsigned i;
  for (i = 0; i < sizeof(buttons) / sizeof(unsigned); ++i) {
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = buttons[i],
        .active_level = 0,
    };
    button_handle_t gpio_btn = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &gpio_btn);
    ESP_ERROR_CHECK(ret);
    if (NULL == gpio_btn) {
      ESP_LOGE(TAG, "Button create failed");
    }
    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, NULL,
                           button_single_click_cb, (void *)buttons[i]);
  }
}