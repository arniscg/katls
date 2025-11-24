#include "wifi.h"
#include "esp_dpp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "qrcode.h"

static const char *TAG = "wifi";

wifi_config_t s_dpp_wifi_config;

static int s_retry_num = 0;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_dpp_event_group;

#define DPP_CONNECTED_BIT BIT0
#define DPP_CONNECT_FAIL_BIT BIT1
#define DPP_AUTH_FAIL_BIT BIT2
#define WIFI_MAX_RETRY_NUM 3

static const char *lt[] = {
    /* 0 */ "  ",
    /* 1 */ "\u2580 ",
    /* 2 */ " \u2580",
    /* 3 */ "\u2580\u2580",
    /* 4 */ "\u2584 ",
    /* 5 */ "\u2588 ",
    /* 6 */ "\u2584\u2580",
    /* 7 */ "\u2588\u2580",
    /* 8 */ " \u2584",
    /* 9 */ "\u2580\u2584",
    /* 10 */ " \u2588",
    /* 11 */ "\u2580\u2588",
    /* 12 */ "\u2584\u2584",
    /* 13 */ "\u2588\u2584",
    /* 14 */ "\u2584\u2588",
    /* 15 */ "\u2588\u2588",
};

void qrcode_draw_func(esp_qrcode_handle_t qrcode) {
  ESP_LOGI(TAG, "draw func");
  int size = esp_qrcode_get_size(qrcode);
  int border = 2;
  unsigned char num = 0;

  for (int y = -border; y < size + border; y += 2) {
    for (int x = -border; x < size + border; x += 2) {
      num = 0;
      if (esp_qrcode_get_module(qrcode, x, y)) {
        num |= 1 << 0;
      }
      if ((x < size + border) && esp_qrcode_get_module(qrcode, x + 1, y)) {
        num |= 1 << 1;
      }
      if ((y < size + border) && esp_qrcode_get_module(qrcode, x, y + 1)) {
        num |= 1 << 2;
      }
      if ((x < size + border) && (y < size + border) &&
          esp_qrcode_get_module(qrcode, x + 1, y + 1)) {
        num |= 1 << 3;
      }
      printf("%s", lt[num]);
    }
    printf("\n");
  }
  printf("\n");
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      ESP_ERROR_CHECK(esp_supp_dpp_start_listen());
      ESP_LOGI(TAG, "Started listening for DPP Authentication");
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      if (s_retry_num < WIFI_MAX_RETRY_NUM) {
        esp_wifi_connect();
        s_retry_num++;
        ESP_LOGI(TAG, "Disconnect event, retry to connect to the AP");
      } else {
        xEventGroupSetBits(s_dpp_event_group, DPP_CONNECT_FAIL_BIT);
      }
      break;
    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "Successfully connected to the AP ssid : %s ",
               s_dpp_wifi_config.sta.ssid);
      break;
    case WIFI_EVENT_DPP_URI_READY:
      wifi_event_dpp_uri_ready_t *uri_data = event_data;
      if (uri_data != NULL) {
        esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
        cfg.display_func = &qrcode_draw_func;

        ESP_LOGI(TAG, "Scan below QR Code to configure the enrollee:");
        esp_qrcode_generate(&cfg, (const char *)uri_data->uri);
      }
      break;
    case WIFI_EVENT_DPP_CFG_RECVD:
      wifi_event_dpp_config_received_t *config = event_data;
      memcpy(&s_dpp_wifi_config, &config->wifi_cfg, sizeof(s_dpp_wifi_config));
      s_retry_num = 0;
      esp_wifi_set_config(WIFI_IF_STA, &s_dpp_wifi_config);
      esp_wifi_connect();
      break;
    case WIFI_EVENT_DPP_FAILED:
      wifi_event_dpp_failed_t *dpp_failure = event_data;
      if (s_retry_num < 5) {
        ESP_LOGI(TAG, "DPP Auth failed (Reason: %s), retry...",
                 esp_err_to_name((int)dpp_failure->failure_reason));
        ESP_ERROR_CHECK(esp_supp_dpp_start_listen());
        s_retry_num++;
      } else {
        xEventGroupSetBits(s_dpp_event_group, DPP_AUTH_FAIL_BIT);
      }

      break;
    default:
      break;
    }
  }
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_dpp_event_group, DPP_CONNECTED_BIT);
  }
}

static void dpp_enrollee_init() {
  s_dpp_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_supp_dpp_init());
  ESP_ERROR_CHECK(
      esp_supp_dpp_bootstrap_gen("4", DPP_BOOTSTRAP_QR_CODE, NULL, NULL));
  ESP_ERROR_CHECK(esp_wifi_start());

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
   * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
   * bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(
      s_dpp_event_group,
      DPP_CONNECTED_BIT | DPP_CONNECT_FAIL_BIT | DPP_AUTH_FAIL_BIT, pdFALSE,
      pdFALSE, portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we
   * can test which event actually happened. */
  if (bits & DPP_CONNECTED_BIT) {
  } else if (bits & DPP_CONNECT_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
             s_dpp_wifi_config.sta.ssid, s_dpp_wifi_config.sta.password);
  } else if (bits & DPP_AUTH_FAIL_BIT) {
    ESP_LOGI(TAG, "DPP Authentication failed after %d retries", s_retry_num);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  esp_supp_dpp_deinit();
  ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler));
  vEventGroupDelete(s_dpp_event_group);
}

void wifi_task(void *) {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  dpp_enrollee_init();

  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}