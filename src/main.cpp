#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "types.h"
#include "network.h"
#include "node_engine.h"
#include "sensors.h"
#include "sim.h"
#include "display.h"
#include "commands.h"
#include "water_speed.h"
#include "phone_messages.h"
#include "bluetooth.h"

#define MAX_EVENTS 8
#define MAX_SPEEDS 4
#define REQUEST_SIZE 32

struct River_network network;       // every node on the river, including this one
struct Node_engine local_engine;    // turns this board's readings into reports
struct Speed_tracker speed_tracker; // water speed from pollution moving between nodes

unsigned long last_sample_ms = 0;
unsigned long last_minute_ms = 0;
unsigned long minute_index = 0;     // minutes finished since turning on
unsigned long sample_in_minute = 0;

// The newest report, kept so a phone that connects can be sent it straight away
struct Node_report latest_report;
struct Network_assessment latest_assessment;
bool have_report = false;
char phone_line[BLE_LINE_SIZE];    // one message for the phone

////////////////////////
// Function prototypes//
////////////////////////
void setup_river_layout(void);
void check_phone(void);
void send_report_to_phone(void);
void send_minute_to_phone(struct Alert_event events[], int num_events, struct Speed_estimate speeds[],
    int num_speeds);
////////////////////////

void setup() {
    Serial.begin(115200);
    delay(200);

    setup_river_layout();
    engine_begin(&local_engine, NODE_ID);
    speed_tracker_begin(&speed_tracker);

#if !SIMULATE
    wake_up_sensors();
#endif

#if BLUETOOTH
    bluetooth_begin(NODE_ID);
#endif

    print_banner(&network);
    last_sample_ms = millis();
    last_minute_ms = last_sample_ms;
}

void loop() {
    check_serial_commands(&network, minute_index);
    check_phone();
    unsigned long now = millis();

    // Take a sample every 2 s
    if (now - last_sample_ms >= SAMPLE_INTERVAL_MS) {
        last_sample_ms += SAMPLE_INTERVAL_MS;
#if SIMULATE
        struct Reading reading = sim_reading(SIM_LOCAL, minute_index, sample_in_minute);
#else
        struct Reading reading = read_sensors();
#endif
        sample_in_minute++;
        engine_add_sample(&local_engine, reading);
    }

    // Every minute: work out what the water is doing and print it
    if (now - last_minute_ms >= MINUTE_MS) {
        last_minute_ms += MINUTE_MS;
        unsigned long uptime_sec = now / 1000;

        struct Node_report report;
        struct Alert_event events[MAX_EVENTS];
        int num_events = engine_close_minute(&local_engine, uptime_sec, &report, events, MAX_EVENTS);

#if SIM_REMOTE_NODES
        sim_tick(minute_index, uptime_sec, &network, minute_index + 1);
#endif
        minute_index++;
        sample_in_minute = 0;
        network_update_report(&network, NODE_ID, report, minute_index);

        struct Speed_estimate speeds[MAX_SPEEDS];
        int num_speeds = speed_tracker_update(&speed_tracker, &network, minute_index, speeds, MAX_SPEEDS);

        print_alert_events(events, num_events);
        print_speed_estimates(speeds, num_speeds);
        struct Network_assessment assessment = network_assess(&network, NODE_ID, minute_index);
        print_minute_report(report, assessment, &network, minute_index);

        latest_report = report;
        latest_assessment = assessment;
        have_report = true;
        send_minute_to_phone(events, num_events, speeds, num_speeds);
    }
}

/////////////////////////
// Function definitions//
/////////////////////////

// The nodes on the river, from upstream to downstream. Two nodes at the same
// distance are on opposite banks at the same spot. More nodes can be added
// while it's running with the "add" serial command.
void setup_river_layout(void) {
    network_add_node(&network, "N0", 0, "upstream", false);
    network_add_node(&network, NODE_ID, 1000, "left bank", true);  // this board
    network_add_node(&network, "N2", 1000, "right bank", false);
    network_add_node(&network, "N3", 2500, "downstream", false);

#if SIM_REMOTE_NODES
    // Fake the other nodes, since there's only one real board
    sim_add_node("N0", SIM_UPSTREAM);
    sim_add_node("N2", SIM_SIBLING);
    sim_add_node("N3", SIM_DOWNSTREAM);
#endif
}

// A phone that just connected (or asked with "refresh") gets the newest
// report and the node list straight away instead of waiting for the next minute
void check_phone(void) {
    char request[REQUEST_SIZE];
    bool send_now = bluetooth_just_connected();
    if (bluetooth_take_request(request, REQUEST_SIZE) && strcmp(request, "refresh") == 0) {
        send_now = true;
    }
    if (send_now) {
        send_report_to_phone();
    }
}

// The status and node list messages
void send_report_to_phone(void) {
    if (!have_report || !bluetooth_connected()) {
        return;
    }
    int length = build_status_message(phone_line, BLE_LINE_SIZE, latest_report, latest_assessment,
                                      &speed_tracker, minute_index);
    bluetooth_send_line(phone_line, length);
    length = build_nodes_message(phone_line, BLE_LINE_SIZE, &network, minute_index);
    bluetooth_send_line(phone_line, length);
}

// Everything new this minute: alerts first (so the phone can buzz straight
// away), then any new water speeds, then the full status
void send_minute_to_phone(struct Alert_event events[], int num_events, struct Speed_estimate speeds[],
    int num_speeds) {
    if (!bluetooth_connected()) {
        return;
    }
    for (int i = 0; i < num_events; i++) {
        if (is_alert_label(events[i].id)) {
            int length = build_alert_message(phone_line, BLE_LINE_SIZE, events[i], latest_report,
                                             minute_index);
            bluetooth_send_line(phone_line, length);
        }
    }
    for (int i = 0; i < num_speeds; i++) {
        int length = build_speed_message(phone_line, BLE_LINE_SIZE, speeds[i], minute_index);
        bluetooth_send_line(phone_line, length);
    }
    send_report_to_phone();
}
