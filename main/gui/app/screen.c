#include "screen.h"
#include "../../buttons.h"
#include "esp_log.h"

static const char *TAG = "screen";

void scr_splash_create(SplashScreen *s) {
  ESP_LOGI(TAG, "scr_splash_create");
  s->screen = lv_obj_create(NULL);

  lv_obj_set_style_bg_color(s->screen, lv_color_hex(0x003a57), LV_PART_MAIN);

  s->lbl = lv_label_create(s->screen);
  lv_label_set_text(s->lbl, "KATLS");
  lv_obj_set_style_text_color(s->screen, lv_color_hex(0xffffff), LV_PART_MAIN);
  lv_obj_align(s->lbl, LV_ALIGN_CENTER, 0, 0);
}

void scr_splash_destroy(SplashScreen *s) {
  ESP_LOGI(TAG, "scr_splash_destroy");
  if (s->screen)
    lv_obj_delete(s->screen);
}

static void scr_prompt_update(PromptScreen *s) {
  switch (s->state) {
  case SCR_PROMPT_TEMP: {
    char str[4];
    lv_snprintf(str, 4, "%dC", s->temp);
    lv_label_set_text(s->lbl_prompt, "set temperature:");
    lv_label_set_text(s->lbl_value, str);
    break;
  }
  case SCR_PROMPT_BAGS: {
    char str[3];
    lv_snprintf(str, 3, "%d", s->bags);
    lv_label_set_text(s->lbl_prompt, "added bags:");
    lv_label_set_text(s->lbl_value, str);
    break;
  }
  case SCR_PROMPT_CLEAN:
    lv_label_set_text(s->lbl_prompt, "cleaning");
    lv_label_set_text(s->lbl_value, "done");
    break;
  default:
    break;
  }
}

void scr_prompt_create(PromptScreen *s, uint8_t but) {
  ESP_LOGI(TAG, "scr_prompt_create");
  s->screen = lv_obj_create(NULL);
  lv_obj_set_scrollbar_mode(s->screen, LV_SCROLLBAR_MODE_OFF);

  s->cont = lv_obj_create(s->screen);
  lv_obj_set_scrollbar_mode(s->cont, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_size(s->cont, 160, 128);
  lv_obj_set_flex_flow(s->cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_align(s->cont, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_align(s->cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  s->lbl_prompt = lv_label_create(s->cont);
  s->lbl_value = lv_label_create(s->cont);

  s->temp = 40;
  s->bags = 1;
  s->timer = NULL;

  if (but == BUTTON_F1) {
    s->state = SCR_PROMPT_TEMP;
  } else if (but == BUTTON_F2) {
    s->state = SCR_PROMPT_BAGS;
  } else if (but == BUTTON_F3) {
    s->state = SCR_PROMPT_CLEAN;
  } else {
    s->state = SCR_PROMPT_NONE;
  }
  scr_prompt_update(s);
}

void scr_prompt_destroy(PromptScreen *s) {
  ESP_LOGI(TAG, "scr_prompt_destroy");
  if (s->screen)
    lv_obj_delete(s->screen);
  if (s->timer)
    xTimerDelete(s->timer, 1000);
}

void scr_prompt_handle_button(PromptScreen *s, uint8_t but) {
  ESP_LOGI(TAG, "scr_prompt_handle_button");

  switch (s->state) {
  case SCR_PROMPT_TEMP:
    if (but == BUTTON_FORWARD) {
      if (s->temp < 70)
        s->temp += 5;
    } else if (but == BUTTON_BACK) {
      if (s->temp > 40)
        s->temp -= 5;
    }
    break;
  case SCR_PROMPT_BAGS:
    if (but == BUTTON_FORWARD) {
      if (s->bags < 12)
        s->bags++;
    } else if (but == BUTTON_BACK) {
      if (s->bags > 1)
        s->bags--;
    }
    break;
  case SCR_PROMPT_CLEAN:
    break;
  default:
    break;
  }
  scr_prompt_update(s);
}

void scr_prompt_handle_ok_cancel(PromptScreen *s, uint8_t b,
                                 TimerCallbackFunction_t cb) {
  lv_color_t color;
  if (b == BUTTON_OK)
    color = lv_color_hex(0x0000ff);
  else if (b == BUTTON_CANCEL)
    color = lv_color_hex(0xff0000);
  else
    return;

  lv_obj_set_style_bg_color(s->cont, color, LV_PART_MAIN);

  s->timer = xTimerCreate("ok_cancel_timer", pdMS_TO_TICKS(1000), pdFALSE,
                          (void *)0, cb);
  xTimerStart(s->timer, 0);
}
