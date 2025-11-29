#include "../../buttons.h"
#include "esp_log.h"
#include "lvgl.h"
#include "screen.h"

static const char *TAG = "GUI APP";

typedef struct {
  SplashScreen *scr_splash;
  PromptScreen *scr_prompt;
  QRCodeScreen *scr_qr;
} App;

static App app;

void gui() {
  ESP_LOGI(TAG, "gui");

  app.scr_splash = NULL;
  app.scr_prompt = NULL;
  app.scr_qr = NULL;

  // app.scr_splash = malloc(sizeof(SplashScreen));
  // scr_splash_create(app.scr_splash);
  // lv_screen_load(app.scr_splash->screen);

  app.scr_qr = malloc(sizeof(QRCodeScreen));
  scr_qr_create(app.scr_qr);
  lv_screen_load(app.scr_qr->screen);
}

static void prompt_callback(TimerHandle_t) {
  ESP_LOGI(TAG, "");
  if (app.scr_prompt) {
    scr_prompt_destroy(app.scr_prompt);
    app.scr_prompt = NULL;
  }

  if (!app.scr_splash) {
    app.scr_splash = malloc(sizeof(SplashScreen));
    scr_splash_create(app.scr_splash);
  }

  lv_screen_load(app.scr_splash->screen);
}

void gui_button(uint8_t b) {
  if (b == BUTTON_F1 || b == BUTTON_F2 || b == BUTTON_F3 || b == BUTTON_F4) {
    if (app.scr_splash) {
      scr_splash_destroy(app.scr_splash);
      app.scr_splash = NULL;
    }

    if (app.scr_prompt)
      scr_prompt_destroy(app.scr_prompt);
    app.scr_prompt = malloc(sizeof(PromptScreen));
    scr_prompt_create(app.scr_prompt, b);
    lv_screen_load(app.scr_prompt->screen);
    return;
  }

  if (b == BUTTON_OK || b == BUTTON_CANCEL) {
    if (app.scr_prompt) {
      scr_prompt_handle_ok_cancel(app.scr_prompt, b, prompt_callback);
    }
    return;
  }

  if (b == BUTTON_FORWARD || b == BUTTON_BACK) {
    if (app.scr_prompt) {
      scr_prompt_handle_button(app.scr_prompt, b);
    }
    return;
  }

  ESP_LOGI(TAG, "unhandled button %d", b);
}

void gui_set_qrcode(uint8_t *buf, unsigned w, unsigned h) {
  if (!app.scr_qr)
    return;

  scr_qr_set_bitmap(app.scr_qr, buf, w, h);
}