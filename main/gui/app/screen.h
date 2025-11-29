#ifndef GUI_SCREEN_H
#define GUI_SCREEN_H

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "lvgl.h"

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *lbl;
} SplashScreen;

void scr_splash_create(SplashScreen *);
void scr_splash_destroy(SplashScreen *);

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *cont;
  lv_obj_t *lbl_prompt;
  lv_obj_t *lbl_value;

  enum {
    SCR_PROMPT_NONE,
    SCR_PROMPT_TEMP,
    SCR_PROMPT_BAGS,
    SCR_PROMPT_CLEAN,
  } state;

  int temp;
  int bags;
  TimerHandle_t timer;
} PromptScreen;

void scr_prompt_create(PromptScreen *, uint8_t);
void scr_prompt_destroy(PromptScreen *);
void scr_prompt_handle_button(PromptScreen *, uint8_t);
void scr_prompt_handle_ok_cancel(PromptScreen *, uint8_t,
                                 TimerCallbackFunction_t);

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *qrcode;
  lv_image_dsc_t dsc;
  uint16_t *img_buf;
} QRCodeScreen;

void scr_qr_create(QRCodeScreen *);
void scr_qr_destroy(QRCodeScreen *);
void scr_qr_set_bitmap(QRCodeScreen *, uint8_t *, unsigned, unsigned);

#endif