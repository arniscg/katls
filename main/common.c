#include "common.h"

size_t timeStr(time_t *now, char *buf, size_t len, bool sh) {
  struct tm timeinfo;
  localtime_r(now, &timeinfo);
  if (sh)
    return strftime(buf, len, "%d.%m %H:%M", &timeinfo);
  else
    return strftime(buf, len, "%d.%m.%Y %H:%M", &timeinfo);
}