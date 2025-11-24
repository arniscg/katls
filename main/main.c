/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "buttons.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gui/gui.h"
// #include "journal.h"
#include "sdkconfig.h"
#include "wifi.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

#define GUI_TASK_STACK_SIZE (4 * 1024)
#define GUI_TASK_PRIORITY 2

#define WIFI_TASK_STACK_SIZE (4 * 1024)
#define WIFI_TASK_PRIORITY 2

static const char *TAG = "main";

void app_main(void) {
  ESP_LOGI(TAG, "app_main");
  // journal_init();
  buttons_init();

  xTaskCreate(gui_task, "LVGL", GUI_TASK_STACK_SIZE, NULL, GUI_TASK_PRIORITY,
              NULL);

  xTaskCreate(wifi_task, "WiFi", WIFI_TASK_STACK_SIZE, NULL, WIFI_TASK_PRIORITY,
              NULL);

  // FILE *jf = journal_read_init();
  // if (!jf) {
  //   ESP_LOGE(TAG, "failed to open journal");
  //   return;
  // }

  // char buf[32];
  // BaseEntry *e = NULL;
  // while (journal_next_entry(jf, &e)) {
  //   journal_entry_to_str(e, buf, sizeof(buf));
  //   ESP_LOGI(TAG, "journal entry: %s", buf);
  //   free(e);
  // }
}