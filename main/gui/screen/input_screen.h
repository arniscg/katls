#pragma once

#include "base.h"

typedef struct {
  BaseScreen base;
  lv_obj_t *cont;
  lv_obj_t *lbl_prompt;
  lv_obj_t *lbl_value;

  enum {
    INPUT_NONE,
    INPUT_TEMP,
    INPUT_BAGS,
    INPUT_CLEAN,
    INPUT_STOP,
    SENDING,
    SENDING_FAILED,
    SENDING_SUCCESS,
  } state;

  int temp;
  int bags;
  TimerHandle_t timer;
  uint8_t reqid;
} InputScreen;

void screen_input_init(InputScreen *s);