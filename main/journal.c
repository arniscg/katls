#include "journal.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "journal";
static const char *base_path = "/spiflash";
static const char *journal_path = "/spiflash/journal.bin";
static wl_handle_t wl_handle = WL_INVALID_HANDLE;

void journal_init() {
  ESP_LOGI(TAG, "init");

  const esp_vfs_fat_mount_config_t mount_config = {
      .max_files = 4,
      .format_if_mount_failed = false,
      .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
      .use_one_fat = false,
  };

  esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage",
                                                   &mount_config, &wl_handle);
  ESP_ERROR_CHECK(err);

  // err = esp_vfs_fat_spiflash_unmount_rw_wl(base_path, wl_handle);
  // ESP_ERROR_CHECK(err);
}

FILE *journal_read_init() {
  FILE *f = fopen(journal_path, "r");
  if (f == NULL)
    return NULL;

  char buf[4];
  if (fread(buf, 1, 4, f) != 4) {
    ESP_LOGE(TAG, "init failed");
    fclose(f);
    return NULL;
  }

  if (buf[0] != 'J' || buf[1] != 'R' || buf[2] != 'N' || buf[3] != 'L') {
    ESP_LOGE(TAG, "failed to read magic numbers");
    fclose(f);
    return NULL;
  }

  if (fread(buf, 1, 1, f) != 1) {
    ESP_LOGE(TAG, "init failed");
    fclose(f);
    return NULL;
  }

  if (buf[0] != 1) {
    ESP_LOGE(TAG, "unsupported version");
  }

  return f;
}

bool journal_next_entry(FILE *f, BaseEntry **ret) {
  BaseEntry base;
  if (fread(&base.type, 1, 1, f) != 1)
    return false;
  if (fread(&base.time, 1, 8, f) != 8)
    return false;

  if (base.type == ENTRY_TEMP) {
    TempEntry *e = malloc(sizeof(TempEntry));
    e->b = base;
    if (fread(&e->temp, 1, 1, f) != 1)
      return false;
    *ret = (BaseEntry *)e;
  } else if (base.type == ENTRY_STOP) {
    StopEntry *e = malloc(sizeof(StopEntry));
    e->b = base;
    *ret = (BaseEntry *)e;
  } else if (base.type == ENTRY_BAGS) {
    BagsEntry *e = malloc(sizeof(BagsEntry));
    e->b = base;
    if (fread(&e->count, 1, 1, f) != 1)
      return false;
    *ret = (BaseEntry *)e;
  } else if (base.type == ENTRY_RESTOCK) {
    RestockEntry *e = malloc(sizeof(RestockEntry));
    e->b = base;
    if (fread(&e->count, 1, 2, f) != 2)
      return false;
    *ret = (BaseEntry *)e;
  } else if (base.type == ENTRY_CLEAN) {
    CleanEntry *e = malloc(sizeof(CleanEntry));
    e->b = base;
    *ret = (BaseEntry *)e;
  } else {
    return false;
  }

  fseek(f, 1, SEEK_CUR);
  return true;
}

void journal_entry_to_str(BaseEntry *e, char *buf, size_t len) {
  struct tm *t = localtime((time_t *)&e->time);
  size_t time_sz = strftime(buf, len, "%d.%m.%Y %H:%M", t);

  switch (e->type) {
  case ENTRY_TEMP: {
    TempEntry *x = (TempEntry *)e;
    snprintf(buf + time_sz, len - time_sz, " %uC", x->temp);
    break;
  }
  case ENTRY_STOP: {
    snprintf(buf + time_sz, len - time_sz, " X");
    break;
  }
  case ENTRY_BAGS: {
    BagsEntry *x = (BagsEntry *)e;
    snprintf(buf + time_sz, len - time_sz, " +%u", x->count);
    break;
  }
  case ENTRY_RESTOCK: {
    RestockEntry *x = (RestockEntry *)e;
    snprintf(buf + time_sz, len - time_sz, " G%u", x->count);
    break;
  }
  case ENTRY_CLEAN: {
    snprintf(buf + time_sz, len - time_sz, " P");
    break;
  }
  default:
    break;
  }
}