#include "wifi.h"
#include "common.h"
#include "esp_dpp.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/ringbuf.h"
#include "gui/gui.h"
#include "journal.h"
#include "nvs_flash.h"
#include "qrcode.h"
#include "state.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char *TAG = "wifi";

wifi_config_t s_dpp_wifi_config;

RingbufHandle_t wifi_buf_handle;

static int s_retry_num = 0;

static esp_http_client_handle_t client = NULL;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_dpp_event_group;

#define DPP_CONNECTED_BIT BIT0
#define DPP_CONNECT_FAIL_BIT BIT1
#define DPP_AUTH_FAIL_BIT BIT2
#define WIFI_MAX_RETRY_NUM 3
#define HOSTNAME "rpi5.home.lan"

static void notify_gui_changed() {
  uint8_t msg[] = {GUI_EVT_WIFI_CHANGED};
  if (xRingbufferSend(gui_buf_handle, msg, sizeof(msg), 0) != pdTRUE)
    ESP_LOGE(TAG, "failed to send wifi changed event");
}

void qrcode_draw_func(esp_qrcode_handle_t qrcode) {
  ESP_LOGI(TAG, "draw func");
  int size = esp_qrcode_get_size(qrcode);
  int border = 2;
  assert(size + 2 * border < 255);
  uint8_t size_b = size + 2 * border;

  uint8_t *buf = malloc(size_b * size_b);
  ESP_LOGI(TAG, "buf size=%u", sizeof(buf));
  unsigned i = 0;
  for (int y = -border; y < size + border; ++y) {
    for (int x = -border; x < size + border; ++x) {
      buf[i++] = esp_qrcode_get_module(qrcode, x, y) ? 0 : 255;
    }
  }
  assert(i == size_b * size_b);

  assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
  state.wifi.status = WIFI_STATUS_WAITING_SCAN;
  if (state.wifi.qrcode.data)
    free(state.wifi.qrcode.data);
  state.wifi.qrcode.data = buf;
  state.wifi.qrcode.w = size_b;
  state.wifi.qrcode.h = size_b;
  assert(xSemaphoreGive(state_mutex) == pdTRUE);
  notify_gui_changed();
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      ESP_ERROR_CHECK(esp_supp_dpp_start_listen());
      ESP_LOGI(TAG, "Started listening for DPP Authentication");

      assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
      state.wifi.ip.addr = 0;
      assert(xSemaphoreGive(state_mutex) == pdTRUE);
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      if (s_retry_num < WIFI_MAX_RETRY_NUM) {
        esp_wifi_connect();
        s_retry_num++;
        ESP_LOGI(TAG, "Disconnect event, retry to connect to the AP");
      } else {
        xEventGroupSetBits(s_dpp_event_group, DPP_CONNECT_FAIL_BIT);
      }

      assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
      state.wifi.status = WIFI_STATUS_WAITING_SCAN;
      assert(xSemaphoreGive(state_mutex) == pdTRUE);
      notify_gui_changed();
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
        esp_qrcode_generate(&cfg, (const char *)uri_data->uri);
      }
      break;
    case WIFI_EVENT_DPP_CFG_RECVD:
      wifi_event_dpp_config_received_t *config = event_data;
      memcpy(&s_dpp_wifi_config, &config->wifi_cfg, sizeof(s_dpp_wifi_config));
      s_retry_num = 0;
      esp_wifi_set_config(WIFI_IF_STA, &s_dpp_wifi_config);

      assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
      state.wifi.status = WIFI_STATUS_CONNECTING;
      memcpy(&state.wifi.conf, &s_dpp_wifi_config.sta,
             sizeof(s_dpp_wifi_config.sta));
      assert(xSemaphoreGive(state_mutex) == pdTRUE);
      notify_gui_changed();

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

      assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
      state.wifi.status = WIFI_STATUS_WAITING_SCAN;
      assert(xSemaphoreGive(state_mutex) == pdTRUE);
      notify_gui_changed();
      break;
    default:
      break;
    }
  }
  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;

    assert(xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE);
    state.wifi.status = WIFI_STATUS_CONNECTED;
    state.wifi.ip = event->ip_info.ip;
    assert(xSemaphoreGive(state_mutex) == pdTRUE);
    notify_gui_changed();

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

  // TODO: reconnect logic

  esp_supp_dpp_deinit();
  ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler));
  vEventGroupDelete(s_dpp_event_group);
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
  static char *output_buffer; // Buffer to store response of http request from
                              // event handler
  static int output_len;      // Stores number of bytes read
  if (evt->event_id == HTTP_EVENT_ON_DATA) {
    if (output_len == 0 && evt->user_data) {
      // we are just starting to copy the output data into the use
      memset(evt->user_data, 0, 512);
    }
    /*
     *  Check for chunked encoding is added as the URL for chunked encoding used
     * in this example returns binary data. However, event handler can also be
     * used in case chunked encoding is used.
     */
    if (!esp_http_client_is_chunked_response(evt->client)) {
      // If user_data buffer is configured, copy the response into the buffer
      int copy_len = 0;
      if (evt->user_data) {
        // The last byte in evt->user_data is kept for the NULL character in
        // case of out-of-bound access.
        copy_len = MIN(evt->data_len, (512 - output_len));
        if (copy_len) {
          memcpy(evt->user_data + output_len, evt->data, copy_len);
        }
      } else {
        int content_len = esp_http_client_get_content_length(evt->client);
        if (output_buffer == NULL) {
          // We initialize output_buffer with 0 because it is used by strlen()
          // and similar functions therefore should be null terminated.
          output_buffer = (char *)calloc(content_len + 1, sizeof(char));
          output_len = 0;
          if (output_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
            return ESP_FAIL;
          }
        }
        copy_len = MIN(evt->data_len, (content_len - output_len));
        if (copy_len) {
          memcpy(output_buffer + output_len, evt->data, copy_len);
        }
      }
      output_len += copy_len;
    }
  } else if (evt->event_id == HTTP_EVENT_ON_FINISH) {
    if (output_buffer != NULL) {
      ESP_LOGI(TAG, "resp: %s", output_buffer);
      ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
      free(output_buffer);
      output_buffer = NULL;
    }
    output_len = 0;
  } else if (evt->event_id == HTTP_EVENT_DISCONNECTED) {
    if (output_buffer != NULL) {
      free(output_buffer);
      output_buffer = NULL;
    }
    output_len = 0;
  }
  return ESP_OK;
}

static int http_post(char *data) {
  esp_http_client_config_t config = {.host = HOSTNAME,
                                     .path = "/events",
                                     .port = 8000,
                                     .event_handler = http_event_handler};
  ESP_LOGI(TAG, "post data: %s", data);
  size_t len = strlen(data);
  client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  if (len) {
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, data, len);
  }
  int err = esp_http_client_perform(client);
  esp_http_client_cleanup(client);
  return err;
}

static void handle_send_entry(unsigned entry_id, char *data, size_t size) {
  ESP_LOGI(TAG, "handle_send_entry");

  int ret = http_post(data);

  assert(xSemaphoreTake(journal_mutex, portMAX_DELAY) == pdTRUE);

  BaseEntry *e = journal_find_entry(entry_id);
  if (!e) {
    ESP_LOGE(TAG, "entry %u not found", entry_id);
    assert(xSemaphoreGive(journal_mutex) == pdTRUE);
    return;
  }

  if (ret == ESP_OK) {
    int status = esp_http_client_get_status_code(client);
    int64_t len = esp_http_client_get_content_length(client);

    ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %" PRId64, status,
             len);
    if (status == 200 || status == 201) {
      ESP_LOGI(TAG, "store success %u", entry_id);
      entry_state_update(e, ENTRY_STATE_STORED);
    } else {
      ESP_LOGI(TAG, "store failed %u", entry_id);
    }
  } else {
    ESP_LOGE(TAG, "HTTP POST request failed");
    entry_state_update(e, ENTRY_STATE_STORE_FAILED);
  }

  assert(xSemaphoreGive(journal_mutex) == pdTRUE);
}

static void handle_msg(char *data, size_t size) {
  if (size < 1) {
    ESP_LOGE(TAG, "msg too short");
    return;
  }

  WifiMsg msg = (WifiMsg)data[0];
  ESP_LOGI(TAG, "handle msg %d", msg);

  if (msg == WIFI_HTTP_SEND) {
    if (size < 6) {
      ESP_LOGE(TAG, "http send msg too short");
      return;
    }

    unsigned id = 0;
    memcpy(&id, data + 1, sizeof(id));
    handle_send_entry(id, data + 1 + sizeof(id), size - 1 - sizeof(id));
  } else {
    ESP_LOGE(TAG, "unhandled msg %d", msg);
  }
}

void wifi_task(void *) {
  wifi_buf_handle = xRingbufferCreate(1024, RINGBUF_TYPE_NOSPLIT);
  assert(wifi_buf_handle != NULL);

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  dpp_enrollee_init();

  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&config);

  if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK)
    ESP_LOGE(TAG, "failed to update system time within 10s timeout");

  time_t t = time(NULL);
  char timestr[64];
  timeStr(&t, timestr, sizeof(timestr), false);
  ESP_LOGI(TAG, "current time: %s", timestr);

  size_t item_size;
  char *item = NULL;
  while (1) {
    item = (char *)xRingbufferReceive(wifi_buf_handle, &item_size,
                                      pdMS_TO_TICKS(1000));
    if (item == NULL)
      continue;

    handle_msg(item, item_size);
    vRingbufferReturnItem(wifi_buf_handle, item);
  }
}