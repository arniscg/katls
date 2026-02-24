#pragma once

#include "time.h"

void setTz();
size_t timeStr(time_t *now, char *buf, size_t len, bool sh);

void enter_sleep();

void lock_state();
void unlock_state();