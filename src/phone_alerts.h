#pragma once
// Decides which WiFi pop-up notifications to send each minute, and sends
// them with wifi_alerts.cpp:
//
//   - a pollution or watch alert turning on (with the readings and details)
//   - a new water speed
//   - a sensor problem lasting FAULT_NOTIFY_MIN minutes, but only once the
//     sensors are "armed" (type "arm" when the node is installed), so setting
//     it up and plugging things in doesn't set off notifications
//
// "Armed" is remembered when the board is turned off.
#include "types.h"
#include "network.h"
#include "persistence.h"
#include "water_speed.h"

// Counts how long each sensor has had a problem, so a sensor problem is only
// reported once it has lasted FAULT_NOTIFY_MIN minutes, and only once until
// it's fixed. Index S_COUNT is the pH probe drifting (PH_RECALIBRATE).
struct Fault_watch {
    int minutes[S_COUNT + 1];
    bool reported[S_COUNT + 1];
};

////////////////////////
// Function prototypes//
////////////////////////
void phone_alerts_begin(void);
void phone_alerts_minute(struct Alert_event events[], int num_events, struct Node_report report,
    struct Network_assessment assessment, struct Speed_tracker *speed, struct Speed_estimate speeds[],
    int num_speeds, unsigned long now_min);
void phone_alerts_set_armed(bool armed);
bool phone_alerts_armed(void);
bool phone_alerts_test(void);
const char *phone_alerts_wifi_state(void);

void fault_watch_reset(struct Fault_watch *watch);
int fault_watch_update(struct Fault_watch *watch, struct Node_report report, int sensors[], int max);
////////////////////////
