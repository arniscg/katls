#pragma once

#include "base.h"

typedef struct {
  BaseScreen base;
  lv_obj_t *list;
} JournalScreen;

void screen_journal_init(JournalScreen *s);