#include "buttons.h"
#include "esp_log.h"

// Desktop simulator build: there's no real GPIO hardware to attach to, so
// this is a no-op. Button presses are instead simulated via keyboard, wired
// up in gui.c's linux branch where the SDL window/event context lives.

void buttons_init() {
  ESP_LOGI("BUTTONS-SIM", "no physical buttons in the simulator - use the "
                          "keyboard (see gui.c) to simulate button presses");
}
