#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "network.h"
#include "node_engine.h"
#include "sensors.h"
#include "sim.h"
#include "display.h"
#include "commands.h"

#define MAX_EVENTS 8

struct River_network network;       // every node on the river, including this one
struct Node_engine local_engine;    // turns this board's readings into reports

unsigned long last_sample_ms = 0;
unsigned long last_minute_ms = 0;
unsigned long minute_index = 0;     // minutes finished since turning on
unsigned long sample_in_minute = 0;

////////////////////////
// Function prototypes//
////////////////////////
void setup_river_layout(void);
////////////////////////

void setup() {
    Serial.begin(115200);
    delay(200);

    setup_river_layout();
    engine_begin(&local_engine, NODE_ID);

#if !SIMULATE
    wake_up_sensors();
#endif

    print_banner(&network);
    last_sample_ms = millis();
    last_minute_ms = last_sample_ms;
}

void loop() {
    check_serial_commands(&network, minute_index);
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

        print_alert_events(events, num_events);
        struct Network_assessment assessment = network_assess(&network, NODE_ID, minute_index);
        print_minute_report(report, assessment, &network, minute_index);
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
