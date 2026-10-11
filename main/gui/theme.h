#pragma once

#include <lvgl.h>

// Shared accent palette - mirrors the station web dashboard's colors so the
// whole product reads consistently. Background tints (e.g. for the journal
// list rows) are derived from these at runtime via lv_color_lighten()
// rather than hand-picking separate pastel hex values.
#define KATLS_COLOR_GREEN lv_color_hex(0x2ecc71)
#define KATLS_COLOR_ORANGE lv_color_hex(0xe67e22)
#define KATLS_COLOR_RED lv_color_hex(0xe74c3c)
#define KATLS_COLOR_GRAY lv_color_hex(0x7f8c8d)
#define KATLS_COLOR_BLUE lv_color_hex(0x3498db)
