#include "journal.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENTRY_HDR_LEN 5

static const char *TAG = "journal";
// static const char *base_path = "/spiflash";
// static const char *journal_path = "/spiflash/journal.bin";
// static wl_handle_t wl_handle = WL_INVALID_HANDLE;

// void journal_init() {
//   ESP_LOGI(TAG, "init");

//   const esp_vfs_fat_mount_config_t mount_config = {
//       .max_files = 4,
//       .format_if_mount_failed = false,
//       .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
//       .use_one_fat = false,
//   };

//   esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage",
//                                                    &mount_config, &wl_handle);
//   ESP_ERROR_CHECK(err);

//   // err = esp_vfs_fat_spiflash_unmount_rw_wl(base_path, wl_handle);
//   // ESP_ERROR_CHECK(err);
// }

// FILE *journal_read_init() {
//   FILE *f = fopen(journal_path, "r");
//   if (f == NULL)
//     return NULL;

//   char buf[4];
//   if (fread(buf, 1, 4, f) != 4) {
//     ESP_LOGE(TAG, "init failed");
//     fclose(f);
//     return NULL;
//   }

//   if (buf[0] != 'J' || buf[1] != 'R' || buf[2] != 'N' || buf[3] != 'L') {
//     ESP_LOGE(TAG, "failed to read magic numbers");
//     fclose(f);
//     return NULL;
//   }

//   if (fread(buf, 1, 1, f) != 1) {
//     ESP_LOGE(TAG, "init failed");
//     fclose(f);
//     return NULL;
//   }

//   if (buf[0] != 1) {
//     ESP_LOGE(TAG, "unsupported version");
//   }

//   return f;
// }

// bool journal_next_entry(FILE *f, BaseEntry **ret) {
//   BaseEntry base;
//   if (fread(&base.type, 1, 1, f) != 1)
//     return false;
//   if (fread(&base.time, 1, 8, f) != 8)
//     return false;

//   if (base.type == ENTRY_TEMP) {
//     TempEntry *e = malloc(sizeof(TempEntry));
//     e->b = base;
//     if (fread(&e->temp, 1, 1, f) != 1)
//       return false;
//     *ret = (BaseEntry *)e;
//   } else if (base.type == ENTRY_STOP) {
//     StopEntry *e = malloc(sizeof(StopEntry));
//     e->b = base;
//     *ret = (BaseEntry *)e;
//   } else if (base.type == ENTRY_BAGS) {
//     BagsEntry *e = malloc(sizeof(BagsEntry));
//     e->b = base;
//     if (fread(&e->count, 1, 1, f) != 1)
//       return false;
//     *ret = (BaseEntry *)e;
//   } else if (base.type == ENTRY_RESTOCK) {
//     RestockEntry *e = malloc(sizeof(RestockEntry));
//     e->b = base;
//     if (fread(&e->count, 1, 2, f) != 2)
//       return false;
//     *ret = (BaseEntry *)e;
//   } else if (base.type == ENTRY_CLEAN) {
//     CleanEntry *e = malloc(sizeof(CleanEntry));
//     e->b = base;
//     *ret = (BaseEntry *)e;
//   } else {
//     return false;
//   }

//   fseek(f, 1, SEEK_CUR);
//   return true;
// }

typedef struct {
  BaseEntry *entries;
  unsigned maxsize;
  unsigned currsize;
  unsigned idx;
} Journal;
static bool initdone = false;
static unsigned id = 0;
static Journal j;

void journal_init(unsigned maxsize) {
  if (initdone) {
    ESP_LOGE(TAG, "journal_init: already initialized");
    return;
  }
  initdone = true;
  j.entries = (BaseEntry *)malloc(maxsize * sizeof(BaseEntry *));
  j.maxsize = maxsize;
  j.currsize = 0;
  j.idx = 0;
}

unsigned journal_idx_next() {
  return j.idx == j.maxsize - 1 ? 0 : j.idx + 1;
}

bool journal_add(BaseEntry *e) {
  unsigned nextidx = journal_idx_next();

  if (j.currsize == j.maxsize) {
    // remove oldest
    BaseEntry *del = j.entries[nextidx];
    assert(del != NULL);

    if (del->state != ENTRY_STATE_STORED) {
      ESP_LOGE(TAG, "cannot free journal space, oldest entry not stored");
      return false;
    }

    free(del);
    j.entries[nextidx] = NULL:
    --j.currsize;
  }

  assert(j.entries[nextidx] == NULL);
  j.entries[nextidx] = e;

  ESP_LOGI(TAG, "entry added");
  return true;
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

uint8_t entry_create(BaseEntry *ret, uint8_t *data, size_t size) {
  if (size < ENTRY_HDR_LEN) {
    ESP_LOGW(TAG, "entry to short, len: %d", size);
    return ENTRY_TYPE_UNKNOWN;
  }

  BaseEntry base;
  base.id = ++id;
  base.type = data[0];
  memcpy(&base.time, data + 1, sizeof(base.time));
  base.state = ENTRY_STATE_INIT;
  base.storetry = 0;

  switch (base.type) {
  case ENTRY_TYPE_TEMP: {
    if (size < ENTRY_HDR_LEN + 1) return ENTRY_TYPE_UNKNOWN;
    TempEntry *e = (TempEntry*)malloc((sizeof(TempEntry)));
    e->b = base;
    e->temp = data[5];
    ret = (BaseEntry*)e;
    break;
  }
  case ENTRY_TYPE_STOP: {
    StopEntry *e = (StopEntry*)malloc((sizeof(StopEntry)));
    e->b = base;
    ret = (BaseEntry*)e;
    break;
  }
  case ENTRY_TYPE_BAGS: {
    if (size < ENTRY_HDR_LEN + 1) return ENTRY_TYPE_UNKNOWN;
    BagsEntry *e = (BagsEntry*)malloc((sizeof(BagsEntry)));
    e->b = base;
    e->count = data[5];
    ret = (BaseEntry*)e;
    break;
  }
  case ENTRY_TYPE_RESTOCK: {
    if (size < ENTRY_HDR_LEN + 1) return ENTRY_TYPE_UNKNOWN;
    RestockEntry *e = (RestockEntry*)malloc((sizeof(RestockEntry)));
    e->b = base;
    e->count = data[5];
    ret = (BaseEntry*)e;
    break;
  }
  case ENTRY_TYPE_CLEAN: {
    CleanEntry *e = (CleanEntry*)malloc((sizeof(CleanEntry)));
    e->b = base;
    ret = (BaseEntry*)e;
    break;
  }
  default:
    break;
  }

  char str[256];
  entry_to_str(ret, str, sizeof(str));
  ESP_LOGI(TAG, "entry created: %s", str);

  return ret->b.type;
}

void entry_store(BaseEntry *e) {
  assert(e->state == ENTRY_STATE_INIT || e->state == ENTRY_STATE_STORING);

  e->state = ENTRY_STATE_STORING;
  ++e->storetry;
  if (e->storetry == 3) {
    ESP_LOGW(TAG, "entry %u store failed after 3 tries", e->id);
    e->state = ENTRY_STATE_STORE_FAILED;
    return;
  }

  char req[256];
  char data[128];
  char name[16];

  if (e->type == ENTRY_TYPE_TEMP) {
    name = "set-temp\0";
  } else if (e->type == ENTRY_TYPE_STOP) {
    name = "off\0";
  } else if (e->type == ENTRY_TYPE_BAGS) {
    name = "add-bags\0";
  } else if (e->type == ENTRY_TYPE_RESTOCK) {
    name = "restock\0";
  } else if (e->type == ENTRY_TYPE_CLEAN) {
    name = "clean\0";
  }

  entry_data_str(e,  data, sizeof(data));

  if (strlen(data)) {
    snprintf(req, sizeof(req),
      "{\"id\":\"%u\",\"time\":\"%u\"},\"event\":\"%s\",\"data\":\"%s\"}",
      e->id, e->time, name, data);
  } else {
    snprintf(req, sizeof(req),
      "{\"id\":\"%u\",\"time\":\"%u\"},\"event\":\"set-temp\"}",
      e->id, e->time, name);
  }

  char msg[512];
  msg[0] = WIFI_HTTP_SEND;
  msg[1] = e->id;
  strcpy(msg + 2, req, sizeof(msg));

  if (xRingbufferSend(wifi_buf_handle, msg, sizeof(msg), 0) != pdTRUE) {
    ESP_LOGE(TAG, "failed to send wifi send msg");
    entry_store(e);
    return;
  }

  // timer, retries
}