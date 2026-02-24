/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "buttons.h"
#include "common.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gui/gui.h"
#include "journal.h"
#include "sdkconfig.h"
#include "state.h"
#include "wifi.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <time.h>
#include <unistd.h>

#define GUI_TASK_STACK_SIZE (4 * 1024)
#define GUI_TASK_PRIORITY 2

#define WIFI_TASK_STACK_SIZE (4 * 1024)
#define WIFI_TASK_PRIORITY 2

#define IDLE_TIME 30

static const char *TAG = "main";

void app_main(void) {
  ESP_LOGI(TAG, "app_main");

  setenv("TZ", "UTC-2", 1);
  tzset();

  journal_init();
  state_init();
  buttons_init();

  xTaskCreate(gui_task, "LVGL", GUI_TASK_STACK_SIZE, NULL, GUI_TASK_PRIORITY,
              NULL);

  xTaskCreate(wifi_task, "WiFi", WIFI_TASK_STACK_SIZE, NULL, WIFI_TASK_PRIORITY,
              NULL);

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(5000));

    lock_state();
    time_t last_touched = state.last_touched;
    bool ntp_sync = state.wifi.ntp_sync;
    unlock_state();

    if (!ntp_sync || !last_touched)
      continue;

    unsigned t = time(NULL) - last_touched;

    ESP_LOGI(TAG, "time since last touched: %u", t);

    if (t > IDLE_TIME)
      enter_sleep();
  }
}