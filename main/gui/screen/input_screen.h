#pragma once

#include "base.h"
#include "esp_timer.h"

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
    INPUT_JOURNAL_ADD,
  } state;

  int temp;
  int bags;
  esp_timer_handle_t timer;
} InputScreen;

void screen_input_init(InputScreen *s);