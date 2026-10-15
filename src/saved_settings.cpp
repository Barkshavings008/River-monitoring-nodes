#include "saved_settings.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>

#define SETTINGS_NAME "rivernode"

// Whether sensor-problem notifications are armed (false until "arm" is used)
bool load_armed(void) {
    Preferences settings;
    settings.begin(SETTINGS_NAME, false);
    bool armed = settings.getBool("armed", false);
    settings.end();
    return armed;
}

void save_armed(bool armed) {
    Preferences settings;
    settings.begin(SETTINGS_NAME, false);
    settings.putBool("armed", armed);
    settings.end();
}

#else

bool saved_armed = false;

bool load_armed(void) {
    return saved_armed;
}

void save_armed(bool armed) {
    saved_armed = armed;
}

#endif
