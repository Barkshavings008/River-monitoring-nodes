#include "phone_alerts.h"
#include "phone_messages.h"
#include "wifi_alerts.h"
#include "saved_settings.h"
#include "report_text.h"
#include <stdio.h>
#include <string.h>

#define MAX_NEW_FAULTS (S_COUNT + 1)
#define PRIORITY_INFO 2
#define PRIORITY_TEST 3

struct Fault_watch fault_watch;
bool sensors_armed = false;
bool have_latest = false;
struct Node_report latest;            // newest report, for "test" and "arm"
char notify_line[NOTIFY_SIZE];        // one notification being built

////////////////////////
// Function prototypes//
////////////////////////
bool send_notification(int length);
void say_online(void);
////////////////////////

// Loads "armed", starts WiFi, and queues the "online" notification (it's
// sent as soon as WiFi connects)
void phone_alerts_begin(void) {
    fault_watch_reset(&fault_watch);
    sensors_armed = load_armed();
    memset(&latest, 0, sizeof(struct Node_report));
    strncpy(latest.id, NODE_ID, NODE_ID_LEN - 1);
    latest.state = ST_BASELINE_BUILDING;
    wifi_alerts_begin();
    if (NOTIFY_ON_START) {
        say_online();
    }
}

// Called every minute after the report is made
void phone_alerts_minute(struct Alert_event events[], int num_events, struct Node_report report,
    struct Network_assessment assessment, struct Speed_tracker *speed, struct Speed_estimate speeds[],
    int num_speeds, unsigned long now_min) {
    latest = report;
    have_latest = true;

    int length = build_ntfy_alert(notify_line, NOTIFY_SIZE, NTFY_TOPIC, events, num_events, report,
                                  assessment, speed, now_min);
    send_notification(length);

    length = build_ntfy_speed(notify_line, NOTIFY_SIZE, NTFY_TOPIC, speeds, num_speeds);
    send_notification(length);

    // Sensor problems are always counted, but only sent once armed
    int sensors[MAX_NEW_FAULTS];
    int num_faults = fault_watch_update(&fault_watch, report, sensors, MAX_NEW_FAULTS);
    if (sensors_armed) {
        for (int i = 0; i < num_faults; i++) {
            length = build_ntfy_fault(notify_line, NOTIFY_SIZE, NTFY_TOPIC, report, sensors[i]);
            send_notification(length);
        }
    }
}

// Turns sensor-problem notifications on or off (remembered when turned off),
// and sends a notification saying so. When arming, any sensor that has a
// problem right now is mentioned, so it can be fixed first.
void phone_alerts_set_armed(bool armed) {
    sensors_armed = armed;
    save_armed(armed);

    char title[64];
    char intro[600];
    if (armed) {
        snprintf(title, 64, "Sensors armed at %s", NODE_ID);
        snprintf(intro, 600, "You'll now get a notification if a probe is unplugged, stuck or "
                 "reading impossible values for %d minutes.", FAULT_NOTIFY_MIN);
        for (int s = 0; s < S_COUNT; s++) {
            if (have_latest && latest.fault[s] != F_NONE) {
                int used = strlen(intro);
                snprintf(intro + used, 600 - used, "\nProblem right now: %s - %s", sensor_name(s),
                         fault_reason(s, latest.fault[s]));
            }
        }
    } else {
        snprintf(title, 64, "Sensor alerts off at %s", NODE_ID);
        snprintf(intro, 600, "Sensor-problem notifications are off. Pollution alerts and water "
                 "speeds still come through.");
    }
    int length = build_ntfy_readings(notify_line, NOTIFY_SIZE, NTFY_TOPIC, title, intro, latest,
                                     PRIORITY_INFO, "shield");
    send_notification(length);
}

bool phone_alerts_armed(void) {
    return sensors_armed;
}

// Sends a test notification with the current readings. Returns false if
// WiFi isn't set up (or the queue is full).
bool phone_alerts_test(void) {
    char title[64];
    snprintf(title, 64, "Test from %s", NODE_ID);
    int length = build_ntfy_readings(notify_line, NOTIFY_SIZE, NTFY_TOPIC, title,
                                     "Notifications are working.", latest, PRIORITY_TEST, "bell");
    return send_notification(length);
}

// "off" (no WiFi name/topic in secrets.h), "connecting" or "connected"
const char *phone_alerts_wifi_state(void) {
    if (!wifi_alerts_enabled()) {
        return "off";
    } else if (wifi_alerts_connected()) {
        return "connected";
    } else {
        return "connecting";
    }
}

/////////////////////////
//// SENSOR PROBLEMS ////
/////////////////////////

void fault_watch_reset(struct Fault_watch *watch) {
    for (int s = 0; s <= S_COUNT; s++) {
        watch->minutes[s] = 0;
        watch->reported[s] = false;
    }
}

// Adds this minute's faults. Fills sensors[] with each sensor whose problem
// has just lasted FAULT_NOTIFY_MIN minutes (S_COUNT = pH probe drifting),
// and returns how many. A sensor is only listed again after it's been fixed.
int fault_watch_update(struct Fault_watch *watch, struct Node_report report, int sensors[], int max) {
    int found = 0;
    for (int s = 0; s <= S_COUNT; s++) {
        bool problem;
        if (s < S_COUNT) {
            problem = report.fault[s] != F_NONE;
        } else {
            problem = report.ph_recalibrate;
        }

        if (!problem) {
            watch->minutes[s] = 0;
            watch->reported[s] = false;
            continue;
        }
        if (watch->minutes[s] < FAULT_NOTIFY_MIN) {
            watch->minutes[s]++;
        }
        if (watch->minutes[s] >= FAULT_NOTIFY_MIN && !watch->reported[s] && found < max) {
            watch->reported[s] = true;
            sensors[found] = s;
            found++;
        }
    }
    return found;
}

/////////////////////////
// Function definitions//
/////////////////////////

bool send_notification(int length) {
    if (length <= 0) {
        return false;
    }
    return wifi_alerts_send(notify_line);
}

void say_online(void) {
    char title[64];
    char intro[200];
    snprintf(title, 64, "River node %s is online", NODE_ID);
    if (sensors_armed) {
        snprintf(intro, 200, "Notifications are working. Sensor-problem alerts are armed.");
    } else {
        snprintf(intro, 200, "Notifications are working. Type \"arm\" once the probes are in the "
                 "water to get sensor-problem alerts too.");
    }
    int length = build_ntfy(notify_line, NOTIFY_SIZE, NTFY_TOPIC, title, intro, PRIORITY_INFO,
                            "white_check_mark");
    send_notification(length);
}
