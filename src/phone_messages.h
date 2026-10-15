#pragma once
// Builds the messages sent to the phone over Bluetooth. Each message is one
// line of JSON text (ending in '\n') with a "type" saying what it is:
//
//   status  every minute: this node's 4 readings, normal levels, what it
//           thinks the water is doing, the network finding and water speed
//   alert   straight away when a pollution/watch label turns on or off
//   nodes   every minute: every node on the river and its status
//   speed   when a new water speed has been worked out
//
// It also builds the WiFi pop-up notifications (build_ntfy_...), which are
// JSON messages for ntfy.sh: a title, the text, how urgent it is (1-5) and
// an emoji tag.
//
// The phone page (docs/phone/index.html) reads these. They're plain text, so
// any Bluetooth terminal app can show them too.
// (Only snprintf in here, so it also builds for the PC tests)
#include "types.h"
#include "network.h"
#include "persistence.h"
#include "water_speed.h"

////////////////////////
// Function prototypes//
////////////////////////
int build_status_message(char *out, int size, struct Node_report report,
    struct Network_assessment assessment, struct Speed_tracker *speed, unsigned long now_min,
    const char *wifi_state, bool armed);
int build_alert_message(char *out, int size, struct Alert_event event, struct Node_report report,
    unsigned long now_min);
int build_nodes_message(char *out, int size, struct River_network *network, unsigned long now_min);
int build_speed_message(char *out, int size, struct Speed_estimate estimate, unsigned long now_min);

int build_ntfy(char *out, int size, const char *topic, const char *title, const char *message,
    int priority, const char *tag);
int build_ntfy_alert(char *out, int size, const char *topic, struct Alert_event events[],
    int num_events, struct Node_report report, struct Network_assessment assessment,
    struct Speed_tracker *speed, unsigned long now_min);
int build_ntfy_speed(char *out, int size, const char *topic, struct Speed_estimate speeds[],
    int num_speeds);
int build_ntfy_fault(char *out, int size, const char *topic, struct Node_report report, int sensor);
int build_ntfy_readings(char *out, int size, const char *topic, const char *title,
    const char *intro, struct Node_report report, int priority, const char *tag);
////////////////////////
