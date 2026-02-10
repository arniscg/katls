#include "common.h"

void setTz() {
  setenv("TZ", "Europe/Riga", 1);
  tzset();
}

size_t timeStr(time_t *now, char *buf, size_t len, bool sh) {
  setTz();
  struct tm timeinfo;
  localtime_r(now, &timeinfo);
  if (sh)
    return strftime(buf, len, "%d.%m %H:%M", &timeinfo);
  else
    return strftime(buf, len, "%d.%m.%Y %H:%M", &timeinfo);
}