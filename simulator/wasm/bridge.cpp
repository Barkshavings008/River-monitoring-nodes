// Connects the web simulator to the real node code in src/.
//
// The page works out what the water is like at each node and turns it into
// sensor voltages. Everything after that is the same code the ESP32 runs:
// the voltage conversions (compensation.cpp), the node engine (medians,
// faults, rules, persistence, baseline), the river network comparison
// (network.cpp) and the serial output (display.cpp).
//
// Strings are passed through text_slot(0..3): the page writes into a slot,
// then calls the function.
#include "Arduino.h"
#include "config.h"
#include "types.h"
#include "labels.h"
#include "compensation.h"
#include "node_engine.h"
#include "network.h"
#include "display.h"

#define EXPORT(name) extern "C" __attribute__((export_name(name)))
#define NUM_SLOTS 4
#define SLOT_SIZE 64
#define JSON_SIZE 4096
#define MAX_EVENTS 8

// One monitoring station: its own engine, like one ESP32 board
struct Station {
    bool used;
    char id[NODE_ID_LEN];
    struct Node_engine engine;
    struct Node_report report;
    struct Alert_event events[MAX_EVENTS];
    int num_events;
};

char serial_buffer[SERIAL_BUFFER_SIZE];
int serial_length = 0;
struct Fake_serial Serial;

struct River_network network;
struct Station stations[MAX_NODES];
char slots[NUM_SLOTS][SLOT_SIZE];
char json[JSON_SIZE];
int json_length = 0;

// From display.cpp (not in display.h because the board doesn't need them elsewhere)
void finding_text(struct Network_assessment a, char *buffer, int size);
void label_code(struct Node_report report, char *buffer, int size);
const char *finding_code(enum Net_finding finding);
const char *fault_code_text(enum Fault_code fault);

////////////////////////
// Function prototypes//
////////////////////////
struct Station *find_station(const char *id);
void json_add(const char *text);
void json_add_float(float value);
void json_add_text(const char *key, const char *value, bool comma);
////////////////////////

void serial_add(const char *text) {
    int length = strlen(text);
    if (serial_length + length >= SERIAL_BUFFER_SIZE) {
        return;
    }
    memcpy(serial_buffer + serial_length, text, length);
    serial_length += length;
    serial_buffer[serial_length] = '\0';
}

EXPORT("text_slot") char *text_slot(int slot) {
    return slots[slot];
}

EXPORT("sim_reset") void sim_reset(void) {
    network_clear(&network);
    for (int i = 0; i < MAX_NODES; i++) {
        stations[i].used = false;
    }
}

// slot 0 = name, slot 1 = branch it flows into ("" for none)
EXPORT("sim_add_branch") int sim_add_branch(unsigned length_m, unsigned joins_at_m) {
    return network_add_branch(&network, slots[0], length_m, slots[1], joins_at_m);
}

// slot 0 = id, slot 1 = branch, slot 2 = place. Returns 1 if it worked.
EXPORT("sim_add_node") int sim_add_node(unsigned distance_m) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (!stations[i].used) {
            if (!network_add_node_on_branch(&network, slots[0], slots[1], distance_m, slots[2], false)) {
                return 0;
            }
            stations[i].used = true;
            strncpy(stations[i].id, slots[0], NODE_ID_LEN - 1);
            stations[i].id[NODE_ID_LEN - 1] = '\0';
            engine_begin(&stations[i].engine, slots[0]);
            stations[i].num_events = 0;
            return 1;
        }
    }
    return 0;
}

// slot 0 = id
EXPORT("sim_remove_node") int sim_remove_node(void) {
    struct Station *station = find_station(slots[0]);
    if (station == NULL) {
        return 0;
    }
    station->used = false;
    return network_remove_node(&network, slots[0]);
}

// slot 0 = id, slot 1 = branch. A moved sensor starts learning "normal" again.
EXPORT("sim_move_node") int sim_move_node(unsigned distance_m) {
    struct Station *station = find_station(slots[0]);
    if (station == NULL) {
        return 0;
    }
    engine_begin(&station->engine, station->id);
    return network_move_node_to_branch(&network, slots[0], slots[1], distance_m);
}

// slot 0 = id. One 2 s sample: the same conversions as read_sensors() in sensors.cpp.
// temp_c = -127 means the DS18B20 is unplugged.
EXPORT("sim_add_sample") void sim_add_sample(float ph_volts, float tds_volts, float turbidity_volts,
    float temp_c) {
    struct Station *station = find_station(slots[0]);
    if (station == NULL) {
        return;
    }
    struct Reading reading;
    bool temp_valid = temperature_valid(temp_c);
    float correction_temp = compensation_temp(temp_c, temp_valid);
    reading.value[S_TEMP] = temp_c;
    reading.valid[S_TEMP] = temp_valid;
    reading.value[S_PH] = ph_from_voltage(ph_volts);
    reading.value[S_TDS] = tds_from_voltage(tds_volts, correction_temp);
    reading.value[S_NTU] = ntu_from_voltage(turbidity_volts);
    reading.valid[S_PH] = true;
    reading.valid[S_TDS] = true;
    reading.valid[S_NTU] = true;
    engine_add_sample(&station->engine, reading);
}

// slot 0 = id. Ends the minute for that station and shares its report with the network.
EXPORT("sim_close_minute") void sim_close_minute(unsigned uptime_sec, unsigned now_min) {
    struct Station *station = find_station(slots[0]);
    if (station == NULL) {
        return;
    }
    station->num_events = engine_close_minute(&station->engine, uptime_sec, &station->report,
                                              station->events, MAX_EVENTS);
    network_update_report(&network, station->id, station->report, now_min);
}

// slot 0 = id. What that board would print to its serial monitor this minute.
EXPORT("sim_serial_output") const char *sim_serial_output(unsigned now_min) {
    serial_length = 0;
    serial_buffer[0] = '\0';
    struct Station *station = find_station(slots[0]);
    if (station == NULL) {
        return serial_buffer;
    }
    // Each board thinks of itself as the local node
    for (int i = 0; i < network.num_nodes; i++) {
        network.nodes[i].is_local = strcmp(network.nodes[i].id, station->id) == 0;
    }
    struct Network_assessment assessment = network_assess(&network, station->id, now_min);
    print_alert_events(station->events, station->num_events);
    print_minute_report(station->report, assessment, &network, now_min);
    return serial_buffer;
}

// slot 0 = id. The same minute as data for the page to draw.
EXPORT("sim_station_json") const char *sim_station_json(unsigned now_min) {
    json_length = 0;
    json[0] = '\0';
    struct Station *station = find_station(slots[0]);
    if (station == NULL) {
        json_add("null");
        return json;
    }
    struct Node_report *r = &station->report;
    struct Network_assessment a = network_assess(&network, station->id, now_min);
    struct Label_info info = get_label_info(r->label);
    char code[40];
    char text[220];
    char number[24];
    label_code(*r, code, 40);
    finding_text(a, text, 220);

    json_add("{");
    json_add_text("id", station->id, false);
    json_add_text("state", state_name(r->state), true);
    json_add_text("code", code, true);
    json_add(",\"label\":");
    snprintf(number, 24, "%d", (int)r->label);
    json_add(number);
    json_add_text("name", info.name, true);
    json_add(",\"conf\":");
    json_add_float(r->confidence);

    json_add(",\"now\":[");
    for (int s = 0; s < S_COUNT; s++) {
        if (s > 0) {
            json_add(",");
        }
        if (r->now_valid[s]) {
            json_add_float(r->now[s]);
        } else {
            json_add("null");
        }
    }
    json_add("],\"base\":[");
    for (int s = 0; s < S_COUNT; s++) {
        if (s > 0) {
            json_add(",");
        }
        if (r->base_valid[s]) {
            json_add_float(r->base[s]);
        } else {
            json_add("null");
        }
    }
    json_add("],\"faults\":[");
    for (int s = 0; s < S_COUNT; s++) {
        if (s > 0) {
            json_add(",");
        }
        json_add("\"");
        json_add(fault_code_text(r->fault[s]));
        json_add("\"");
    }
    json_add("],\"also\":[");
    for (int i = 0; i < r->num_also; i++) {
        if (i > 0) {
            json_add(",");
        }
        snprintf(number, 24, "%d", (int)r->also[i]);
        json_add(number);
    }
    json_add("],\"learned\":");
    snprintf(number, 24, "%d,\"needed\":%d", r->learned_minutes, r->learn_needed);
    json_add(number);
    json_add(",\"recal\":");
    if (r->ph_recalibrate) {
        json_add("true");
    } else {
        json_add("false");
    }

    json_add_text("finding", finding_code(a.finding), true);
    json_add_text("finding_text", text, true);
    json_add_text("upstream", a.upstream_id, true);
    json_add(",\"gap\":");
    snprintf(number, 24, "%ld", a.upstream_gap_m);
    json_add(number);

    json_add(",\"events\":[");
    for (int i = 0; i < station->num_events; i++) {
        if (i > 0) {
            json_add(",");
        }
        snprintf(number, 24, "{\"label\":%d,", (int)station->events[i].id);
        json_add(number);
        if (station->events[i].on) {
            json_add("\"on\":true,\"conf\":");
        } else {
            json_add("\"on\":false,\"conf\":");
        }
        json_add_float(station->events[i].confidence);
        json_add("}");
    }
    json_add("]}");
    return json;
}

// The text that goes with a label, from labels.cpp
EXPORT("sim_label_json") const char *sim_label_json(int id) {
    json_length = 0;
    json[0] = '\0';
    struct Label_info info = get_label_info((enum Label_id)id);
    char number[24];
    json_add("{");
    json_add_text("code", info.code, false);
    json_add_text("name", info.name, true);
    json_add_text("short", info.short_name, true);
    snprintf(number, 24, ",\"category\":%d", (int)info.category);
    json_add(number);
    json_add_text("pollutants", info.pollutants, true);
    json_add_text("meaning", info.meaning, true);
    json_add("}");
    return json;
}

// Numbers from config.h the page needs to know
EXPORT("sim_setting") float sim_setting(int which) {
    if (which == 0) {
        return BASELINE_MIN_MINUTES;
    } else if (which == 1) {
        return PERSIST_ON_MIN;
    } else if (which == 2) {
        return PERSIST_OFF_MIN;
    } else if (which == 3) {
        return MAX_NODES;
    } else if (which == 4) {
        return NODE_STALE_MIN;
    } else if (which == 5) {
        return LBL_COUNT;
    } else {
        return 0;
    }
}

/////////////////////////
// Function definitions//
/////////////////////////

struct Station *find_station(const char *id) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (stations[i].used && strcmp(stations[i].id, id) == 0) {
            return &stations[i];
        }
    }
    return NULL;
}

void json_add(const char *text) {
    int length = strlen(text);
    if (json_length + length >= JSON_SIZE) {
        return;
    }
    memcpy(json + json_length, text, length);
    json_length += length;
    json[json_length] = '\0';
}

void json_add_float(float value) {
    char number[24];
    snprintf(number, 24, "%.4f", value);
    json_add(number);
}

// Adds "key":"value" (the texts in this program have no quotes or backslashes in them)
void json_add_text(const char *key, const char *value, bool comma) {
    if (comma) {
        json_add(",");
    }
    json_add("\"");
    json_add(key);
    json_add("\":\"");
    json_add(value);
    json_add("\"");
}
