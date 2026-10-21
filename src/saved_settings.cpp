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

// Whether the alarm servo has been set off (stays set until "reset" is used)
bool load_servo_tripped(void) {
    Preferences settings;
    settings.begin(SETTINGS_NAME, false);
    bool tripped = settings.getBool("servo", false);
    settings.end();
    return tripped;
}

void save_servo_tripped(bool tripped) {
    Preferences settings;
    settings.begin(SETTINGS_NAME, false);
    settings.putBool("servo", tripped);
    settings.end();
}

#else

bool saved_armed = false;
bool saved_servo_tripped = false;

bool load_armed(void) {
    return saved_armed;
}

void save_armed(bool armed) {
    saved_armed = armed;
}

bool load_servo_tripped(void) {
    return saved_servo_tripped;
}

void save_servo_tripped(bool tripped) {
    saved_servo_tripped = tripped;
}

#endif
