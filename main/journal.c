#include "journal.h"
#include "./wifi.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENTRY_HDR_LEN 5
#define JOURNAL_SIZE 16

SemaphoreHandle_t journal_mutex;

static const char *TAG = "journal";

typedef struct {
  BaseEntry *entries[JOURNAL_SIZE];
  unsigned currsize;
  unsigned idx;
} Journal;
static bool initdone = false;
static unsigned id = 0;
static Journal j;

void journal_init() {
  if (initdone) {
    ESP_LOGE(TAG, "journal_init: already initialized");
    return;
  }
  journal_mutex = xSemaphoreCreateMutex();
  initdone = true;
  j.currsize = 0;
  j.idx = 0;

  for (unsigned i = 0; i < JOURNAL_SIZE; ++i)
    j.entries[i] = NULL;
}

unsigned journal_idx_next() {
  return j.idx == JOURNAL_SIZE - 1 ? 0 : j.idx + 1;
}

bool journal_add(BaseEntry *e) {
  unsigned nextidx = journal_idx_next();

  if (j.currsize == JOURNAL_SIZE) {
    // remove oldest
    BaseEntry *del = j.entries[nextidx];
    assert(del != NULL);

    if (del->state != ENTRY_STATE_STORED) {
      ESP_LOGE(TAG, "cannot free journal space, oldest entry not stored");
      return false;
    }

    free(del);
    j.entries[nextidx] = NULL;
    --j.currsize;
  }

  assert(j.entries[nextidx] == NULL);
  j.entries[nextidx] = e;
  j.idx = nextidx;

  ESP_LOGI(TAG, "entry added");
  entry_store(e);
  return true;
}

BaseEntry *journal_find_entry(unsigned id) {
  for (unsigned i = 0; i < JOURNAL_SIZE; ++i) {
    if (j.entries[i] == NULL)
      continue;
    if (j.entries[i]->id == id)
      return j.entries[i];
  }

  return NULL;
}

void entry_to_str(BaseEntry *e, char *buf, size_t len) {
  struct tm *t = localtime((time_t *)&e->time);
  size_t time_sz = strftime(buf, len, "%d.%m.%Y %H:%M", t);

  switch (e->type) {
  case ENTRY_TYPE_TEMP: {
    TempEntry *x = (TempEntry *)e;
    snprintf(buf + time_sz, len - time_sz, " %uC", x->temp);
    break;
  }
  case ENTRY_TYPE_STOP: {
    snprintf(buf + time_sz, len - time_sz, " X");
    break;
  }
  case ENTRY_TYPE_BAGS: {
    BagsEntry *x = (BagsEntry *)e;
    snprintf(buf + time_sz, len - time_sz, " +%u", x->count);
    break;
  }
  case ENTRY_TYPE_RESTOCK: {
    RestockEntry *x = (RestockEntry *)e;
    snprintf(buf + time_sz, len - time_sz, " G%u", x->count);
    break;
  }
  case ENTRY_TYPE_CLEAN: {
    snprintf(buf + time_sz, len - time_sz, " P");
    break;
  }
  default:
    break;
  }
}

void entry_init(BaseEntry *e) {
  e->id = ++id;
  e->time = 0;
  e->state = ENTRY_STATE_INIT;
  e->storetry = 0;
}

uint8_t entry_create(BaseEntry *ret, uint8_t *data, size_t size) {
  if (size < ENTRY_HDR_LEN) {
    ESP_LOGW(TAG, "entry to short, len: %d", size);
    return ENTRY_TYPE_UNKNOWN;
  }

  BaseEntry base;
  entry_init(&base);
  base.type = data[0];
  memcpy(&base.time, data + 1, sizeof(base.time));
  base.storetry = 0;

  switch (base.type) {
  case ENTRY_TYPE_TEMP: {
    if (size < ENTRY_HDR_LEN + 1)
      return ENTRY_TYPE_UNKNOWN;
    TempEntry *e = (TempEntry *)malloc((sizeof(TempEntry)));
    e->b = base;
    e->temp = data[5];
    ret = (BaseEntry *)e;
    break;
  }
  case ENTRY_TYPE_STOP: {
    StopEntry *e = (StopEntry *)malloc((sizeof(StopEntry)));
    e->b = base;
    ret = (BaseEntry *)e;
    break;
  }
  case ENTRY_TYPE_BAGS: {
    if (size < ENTRY_HDR_LEN + 1)
      return ENTRY_TYPE_UNKNOWN;
    BagsEntry *e = (BagsEntry *)malloc((sizeof(BagsEntry)));
    e->b = base;
    e->count = data[5];
    ret = (BaseEntry *)e;
    break;
  }
  case ENTRY_TYPE_RESTOCK: {
    if (size < ENTRY_HDR_LEN + 1)
      return ENTRY_TYPE_UNKNOWN;
    RestockEntry *e = (RestockEntry *)malloc((sizeof(RestockEntry)));
    e->b = base;
    e->count = data[5];
    ret = (BaseEntry *)e;
    break;
  }
  case ENTRY_TYPE_CLEAN: {
    CleanEntry *e = (CleanEntry *)malloc((sizeof(CleanEntry)));
    e->b = base;
    ret = (BaseEntry *)e;
    break;
  }
  default:
    break;
  }

  char str[256];
  entry_to_str(ret, str, sizeof(str));
  ESP_LOGI(TAG, "entry created: %s", str);

  return ret->type;
}

void entry_store(BaseEntry *e) {
  ESP_LOGI(TAG, "entry_store: id %u, try %u", e->id, e->storetry + 1);
  assert(e->state == ENTRY_STATE_INIT || e->state == ENTRY_STATE_STORING);

  e->state = ENTRY_STATE_STORING;
  ++e->storetry;
  if (e->storetry == 3) {
    ESP_LOGW(TAG, "entry %u store failed after 3 tries", e->id);
    e->state = ENTRY_STATE_STORE_FAILED;
    return;
  }

  char req[128];
  char data[32];
  char name[16];

  if (e->type == ENTRY_TYPE_TEMP) {
    snprintf(name, sizeof(name), "%s", "set-temp");
    snprintf(data, sizeof(data), "{\\\"value\\\":%u}", ((TempEntry *)e)->temp);
  } else if (e->type == ENTRY_TYPE_STOP) {
    snprintf(name, sizeof(name), "%s", "off");
    data[0] = '\0';
  } else if (e->type == ENTRY_TYPE_BAGS) {
    snprintf(name, sizeof(name), "%s", "add-bags");
    snprintf(data, sizeof(data), "{\\\"value\\\":%u}", ((BagsEntry *)e)->count);
  } else if (e->type == ENTRY_TYPE_RESTOCK) {
    snprintf(name, sizeof(name), "%s", "restock");
    snprintf(data, sizeof(data), "{\\\"value\\\":%u}", ((RestockEntry *)e)->count);
  } else if (e->type == ENTRY_TYPE_CLEAN) {
    snprintf(name, sizeof(name), "%s", "clean");
    data[0] = '\0';
  }

  if (strlen(data)) {
    snprintf(
        req, sizeof(req),
        "{\"id\":\"%u\",\"time\":%llu,\"event\":\"%s\",\"data\":\"%s\"}",
        e->id, e->time, name, data);
  } else {
    snprintf(req, sizeof(req),
             "{\"id\":\"%u\",\"time\":%llu,\"event\":\"%s\"}", e->id,
             e->time, name);
  }

  ESP_LOGI(TAG, "req data len: %u", strlen(data));

  char msg[256];
  msg[0] = WIFI_HTTP_SEND;
  memcpy(msg + 1, &e->id, sizeof(e->id));
  strcpy(msg + 1 + sizeof(e->id), req);

  if (xRingbufferSend(wifi_buf_handle, msg, sizeof(msg), 0) != pdTRUE) {
    ESP_LOGE(TAG, "failed to send WIFI_HTTP_SEND msg");
    entry_store(e);
    return;
  }
}

void entry_state_update(BaseEntry *e, EntryState s) {
  if (e->state != ENTRY_STATE_STORING) {
    ESP_LOGW(TAG, "state update not expected");
    return;
  }

  if (s == ENTRY_STATE_STORED) {
    ESP_LOGI(TAG, "entry %u stored succesfully", e->id);
    e->state = s;
    return;
  }

  if (s == ENTRY_STATE_STORE_FAILED) {
    ESP_LOGI(TAG, "retry store entry %u", e->id);
    entry_store(e);
  }
}