#include "display.h"
#include "labels.h"
#include <Arduino.h>
#include <string.h>

// Codes that change the text colour in the serial monitor
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define MAGENTA "\033[35m"
#define GREY "\033[90m"
#define COLOUR_END "\033[0m"

#define HEADER_LEFT 16      // "=" signs before the node name in the header
#define HEADER_MIN_RIGHT 3  // at least this many "=" signs after it

////////////////////////
// Function prototypes//
////////////////////////
const char *colour(const char *code);
const char *colour_end(void);
const char *category_colour(enum Category category);
const char *state_colour(enum Node_state state);
const char *category_tag(enum Category category);
const char *fault_code_text(enum Fault_code fault);
const char *fault_reason(int sensor, enum Fault_code fault);
const char *finding_code(enum Net_finding finding);
const char *finding_colour(enum Net_finding finding);
void print_repeated(char c, int n);
void print_padded(const char *text, int width);
void print_right_aligned(unsigned long number, int width);
void print_float(float value, int decimals);
void print_wrapped(const char *lead, const char *text, int width);
void format_value(char *buffer, int size, int sensor, float value, bool valid);
void format_change(char *buffer, int size, int sensor, float now, float base, bool valid);
void label_code(struct Node_report report, char *buffer, int size);
void finding_text(struct Network_assessment a, char *buffer, int size);
void node_status(struct River_network *network, struct River_node *node, unsigned long now_min,
    char *status, int size);
void print_header(struct Node_report report);
void print_readings(struct Node_report report);
void print_faults(struct Node_report report);
void print_label(enum Label_id id, float confidence);
void print_also(struct Node_report report);
void print_disclaimer(void);
void print_network(struct Node_report report, struct Network_assessment a,
    struct River_network *network, unsigned long now_min);
void print_human(struct Node_report report, struct Network_assessment a,
    struct River_network *network, unsigned long now_min);
void print_json_number(float value, bool valid, int decimals);
void print_json_values(float values[], bool valid[]);
void print_json(struct Node_report report, struct Network_assessment a);
////////////////////////

void print_banner(struct River_network *network) {
    const char *mode = "SENSORS";
    if (SIMULATE) {
        mode = "SIMULATE";
    }
    const char *demo = "";
    if (DEMO_MODE) {
        demo = " + DEMO";
    }

    Serial.println();
    print_repeated('=', DISPLAY_WIDTH);
    Serial.println();
    Serial.print("River water-quality node ");
    Serial.println(NODE_ID);
    Serial.print("Mode: ");
    Serial.print(mode);
    Serial.print(demo);
    Serial.print(" | minute = ");
    Serial.print((unsigned long)(MINUTE_MS / 1000));
    Serial.print(" s | baseline ");
    Serial.print((unsigned long)(BASELINE_SLOTS * BASELINE_SLOT_MIN));
    Serial.print(" min, warm-up ");
    Serial.print((unsigned long)BASELINE_MIN_MINUTES);
    Serial.println(" min");
    print_repeated('=', DISPLAY_WIDTH);
    Serial.println();
    print_network_list(network, 0);
    print_help();
}

void print_help(void) {
    Serial.println("Commands: list | add <id> <metres> [place] | del <id> | move <id> <metres> | cal | help");
}

// One line for each label that turned on or off this minute
void print_alert_events(struct Alert_event events[], int num_events) {
    if (OUTPUT_MODE == OUTPUT_JSON) {
        return;
    }
    for (int i = 0; i < num_events; i++) {
        struct Label_info info = get_label_info(events[i].id);
        if (info.category == CAT_POLLUTION || info.category == CAT_WATCH) {
            if (events[i].on) {
                Serial.print(category_colour(info.category));
                Serial.print(">>> ALERT ON: ");
                Serial.print(info.short_name);
                Serial.print(" (conf ");
                print_float(events[i].confidence, 2);
                Serial.print(")");
                Serial.println(colour_end());
            } else {
                Serial.print(colour(GREEN));
                Serial.print(">>> ALERT OFF: ");
                Serial.print(info.short_name);
                Serial.println(colour_end());
            }
        }
    }
}

// The block printed every minute (and/or the JSON line, see OUTPUT_MODE)
void print_minute_report(struct Node_report report, struct Network_assessment assessment,
    struct River_network *network, unsigned long now_min) {
    if (OUTPUT_MODE != OUTPUT_JSON) {
        print_human(report, assessment, network, now_min);
    }
    if (OUTPUT_MODE != OUTPUT_HUMAN) {
        print_json(report, assessment);
    }
}

// Every position on the river and the nodes at it
void print_network_list(struct River_network *network, unsigned long now_min) {
    Serial.print("Network: ");
    Serial.print((unsigned long)network->num_nodes);
    Serial.print(" node(s) at ");
    Serial.print((unsigned long)network_position_count(network));
    Serial.print(" position(s), capacity ");
    Serial.println((unsigned long)MAX_NODES);

    for (int p = 0; p < network_position_count(network); p++) {
        unsigned long distance = network_position_distance(network, p);
        int indexes[MAX_NODES];
        int n = network_nodes_at(network, distance, indexes, MAX_NODES);
        Serial.print("  Position ");
        Serial.print((unsigned long)(p + 1));
        Serial.print(" @ ");
        Serial.print(distance);
        Serial.print(" m:");

        for (int k = 0; k < n; k++) {
            struct River_node *node = &network->nodes[indexes[k]];
            const char *status;
            if (node->is_local) {
                status = "this node";
            } else if (!node->has_report) {
                status = "no data";
            } else if (node_is_fresh(node, now_min)) {
                status = state_name(node->report.state);
            } else {
                status = "offline";
            }

            if (k > 0) {
                Serial.print(",");
            }
            Serial.print(" ");
            Serial.print(node->id);
            Serial.print(" (");
            Serial.print(node->place);
            Serial.print(", ");
            Serial.print(status);
            Serial.print(")");
        }
        Serial.println();
    }
}

void print_message(const char *message) {
    Serial.println(message);
}

void print_voltages(struct Sensor_voltages volts) {
    Serial.print("Calibration voltages: pH ");
    print_float(volts.ph, 3);
    Serial.print(" V | TDS ");
    print_float(volts.tds, 3);
    Serial.print(" V | turbidity ");
    print_float(volts.turbidity, 3);
    Serial.print(" V | temp ");
    print_float(volts.temp_c, 2);
    Serial.println(" C");
    Serial.println("  Set PH_V7 / PH_V4 (in buffers) and TURB_V_CLEAR (clear water) in config.h");
}

/////////////////////////
/////// COLOURS /////////
/////////////////////////

// Gives back the colour code, or "" if colours are turned off
const char *colour(const char *code) {
    if (USE_COLOUR) {
        return code;
    } else {
        return "";
    }
}

const char *colour_end(void) {
    return colour(COLOUR_END);
}

const char *category_colour(enum Category category) {
    if (category == CAT_POLLUTION) {
        return colour(RED);
    } else if (category == CAT_WATCH) {
        return colour(YELLOW);
    } else if (category == CAT_FILTER) {
        return colour(GREY);
    } else if (category == CAT_FAULT) {
        return colour(MAGENTA);
    } else {
        return colour(GREEN);
    }
}

const char *state_colour(enum Node_state state) {
    if (state == ST_ALERT) {
        return colour(RED);
    } else if (state == ST_WATCH) {
        return colour(YELLOW);
    } else if (state == ST_FAULT) {
        return colour(MAGENTA);
    } else if (state == ST_NORMAL) {
        return colour(GREEN);
    } else {
        return colour(GREY);
    }
}

const char *finding_colour(enum Net_finding finding) {
    if (finding == NF_RAIN_UNCONFIRMED) {
        return colour(YELLOW);
    } else if (finding == NF_RAIN_CONFIRMED) {
        return colour(GREY);
    } else if (finding == NF_NO_PEERS) {
        return "";
    } else {
        return colour(RED);
    }
}

/////////////////////////
///////// TEXT //////////
/////////////////////////

const char *category_tag(enum Category category) {
    if (category == CAT_POLLUTION) {
        return "[POLLUTION]";
    } else if (category == CAT_WATCH) {
        return "[WATCH]";
    } else if (category == CAT_FILTER) {
        return "[FILTER]";
    } else if (category == CAT_FAULT) {
        return "[FAULT]";
    } else {
        return "";
    }
}

const char *fault_code_text(enum Fault_code fault) {
    if (fault == F_NO_READING) {
        return "NO_READING";
    } else if (fault == F_OUT_OF_RANGE) {
        return "OUT_OF_RANGE";
    } else if (fault == F_FLATLINE) {
        return "FLATLINE";
    } else if (fault == F_TEMP_JUMP) {
        return "JUMP";
    } else {
        return "NONE";
    }
}

// What the fault means in plain words
const char *fault_reason(int sensor, enum Fault_code fault) {
    if (fault == F_NO_READING) {
        if (sensor == S_TEMP) {
            return "no reading (DS18B20 gave -127/85 C); using 25 C for correction";
        } else {
            return "no reading";
        }
    } else if (fault == F_OUT_OF_RANGE) {
        return "reading out of range - check probe, wiring or calibration";
    } else if (fault == F_FLATLINE) {
        return "reading stuck for 60+ min - check probe and wiring";
    } else if (fault == F_TEMP_JUMP) {
        return "jumped more than 3 C in 1 min - check probe";
    } else {
        return "";
    }
}

const char *finding_code(enum Net_finding finding) {
    if (finding == NF_NO_PEERS) {
        return "NO_PEERS";
    } else if (finding == NF_SOURCE_LOCAL_SIDE) {
        return "SOURCE_LOCAL_SIDE";
    } else if (finding == NF_SOURCE_CROSS_SECTION) {
        return "SOURCE_CROSS_SECTION";
    } else if (finding == NF_SOURCE_BETWEEN) {
        return "SOURCE_BETWEEN";
    } else if (finding == NF_FROM_UPSTREAM) {
        return "FROM_UPSTREAM";
    } else if (finding == NF_RAIN_CONFIRMED) {
        return "RAIN_CONFIRMED";
    } else if (finding == NF_RAIN_UNCONFIRMED) {
        return "RAIN_UNCONFIRMED";
    } else {
        return "NONE";
    }
}

// Writes the report's label code into buffer. For a fault it names the first
// broken sensor, e.g. FAULT_TEMP_NO_READING
void label_code(struct Node_report report, char *buffer, int size) {
    if (report.label != LBL_FAULT) {
        snprintf(buffer, size, "%s", get_label_info(report.label).code);
        return;
    }
    for (int s = 0; s < S_COUNT; s++) {
        if (report.fault[s] != F_NONE) {
            snprintf(buffer, size, "FAULT_%s_%s", sensor_code(s), fault_code_text(report.fault[s]));
            return;
        }
    }
    if (report.ph_recalibrate) {
        snprintf(buffer, size, "PH_RECALIBRATE");
    } else {
        snprintf(buffer, size, "FAULT");
    }
}

// Writes a sentence about what the network comparison found into buffer
// (empty if there's nothing to say)
void finding_text(struct Network_assessment a, char *buffer, int size) {
    unsigned up = (unsigned)a.upstream_distance;
    unsigned here = (unsigned)a.local_distance;
    unsigned rain_nodes = (unsigned)a.rain_peers + 1;
    unsigned all_nodes = (unsigned)a.peers + 1;

    if (a.finding == NF_NO_PEERS) {
        snprintf(buffer, size, "No fresh data from other nodes - cannot compare.");
    } else if (a.finding == NF_SOURCE_LOCAL_SIDE) {
        if (a.has_upstream) {
            snprintf(buffer, size, "Source likely between %s (%u m) and %s (%u m), on %s's side - "
                     "other node(s) at this position are normal.",
                     a.upstream_id, up, a.local_id, here, a.local_id);
        } else {
            snprintf(buffer, size, "Source likely on %s's side at %u m - other node(s) here are normal.",
                     a.local_id, here);
        }
    } else if (a.finding == NF_SOURCE_CROSS_SECTION) {
        if (a.has_upstream) {
            snprintf(buffer, size, "Whole river width affected at %u m; source likely between %s (%u m) "
                     "and here.", here, a.upstream_id, up);
        } else {
            snprintf(buffer, size, "Whole river width affected at %u m.", here);
        }
    } else if (a.finding == NF_SOURCE_BETWEEN) {
        snprintf(buffer, size, "Source likely between %s (%u m) and %s (%u m) - upstream is normal.",
                 a.upstream_id, up, a.local_id, here);
    } else if (a.finding == NF_FROM_UPSTREAM) {
        snprintf(buffer, size, "Same pattern upstream at %s (%u m) - pollution is coming from further "
                 "upstream.", a.upstream_id, up);
    } else if (a.finding == NF_RAIN_CONFIRMED) {
        snprintf(buffer, size, "Rain pattern at %u of %u nodes - confirmed weather event.",
                 rain_nodes, all_nodes);
    } else if (a.finding == NF_RAIN_UNCONFIRMED) {
        snprintf(buffer, size, "Rain pattern at this node only (%u of %u) - possible stormwater outfall "
                 "or leak nearby.", rain_nodes, all_nodes);
    } else {
        buffer[0] = '\0';
    }
}

// Writes one node's status for the network table into status
void node_status(struct River_network *network, struct River_node *node, unsigned long now_min,
    char *status, int size) {
    if (!node->has_report) {
        snprintf(status, size, "no data");
    } else if (!node->is_local && !node_is_fresh(node, now_min)) {
        snprintf(status, size, "offline");
    } else if (node->report.label == LBL_NONE) {
        snprintf(status, size, "%s", state_name(node->report.state));
    } else {
        char code[40];
        label_code(node->report, code, 40);
        snprintf(status, size, "%s %s", state_name(node->report.state), code);
    }
}

// Writes a reading and its unit into buffer, e.g. "210 mg/L"
void format_value(char *buffer, int size, int sensor, float value, bool valid) {
    if (!valid) {
        snprintf(buffer, size, "--");
    } else if (sensor == S_PH) {
        snprintf(buffer, size, "%.2f", value);
    } else if (sensor == S_TDS) {
        snprintf(buffer, size, "%.0f mg/L", value);
    } else if (sensor == S_NTU) {
        snprintf(buffer, size, "%.1f NTU", value);
    } else {
        snprintf(buffer, size, "%.1f C", value);
    }
}

// Writes how far a reading is from normal into buffer, e.g. "+12%" or "x3.2"
void format_change(char *buffer, int size, int sensor, float now, float base, bool valid) {
    if (!valid) {
        snprintf(buffer, size, "--");
    } else if (sensor == S_PH) {
        snprintf(buffer, size, "%+.2f", now - base);
    } else if (sensor == S_TDS) {
        if (base < BASE_FLOOR_TDS) {
            base = BASE_FLOOR_TDS;
        }
        snprintf(buffer, size, "%+.0f%%", (now - base) / base * 100.0f);
    } else if (sensor == S_NTU) {
        if (base < BASE_FLOOR_NTU) {
            base = BASE_FLOOR_NTU;
        }
        snprintf(buffer, size, "x%.1f", now / base);
    } else {
        snprintf(buffer, size, "%+.1f C", now - base);
    }
}

/////////////////////////
//// PRINT HELPERS //////
/////////////////////////

void print_repeated(char c, int n) {
    for (int i = 0; i < n; i++) {
        Serial.print(c);
    }
}

// Prints text, then spaces until it's width characters wide (lines up columns)
void print_padded(const char *text, int width) {
    Serial.print(text);
    print_repeated(' ', width - (int)strlen(text));
}

// Prints spaces, then the number, so it takes up width characters
void print_right_aligned(unsigned long number, int width) {
    int digits = 1;
    unsigned long rest = number / 10;
    while (rest > 0) {
        digits++;
        rest = rest / 10;
    }
    print_repeated(' ', width - digits);
    Serial.print(number);
}

// Prints a float with a set number of decimals. (Serial.print(value, 2)
// rounds slightly differently to snprintf, this keeps the numbers the same as
// the rest of the output.)
void print_float(float value, int decimals) {
    char buffer[24];
    snprintf(buffer, 24, "%.*f", decimals, value);
    Serial.print(buffer);
}

// Prints lead, then the text wrapped onto new lines so it's no wider than
// width. The new lines are lined up under the first word.
void print_wrapped(const char *lead, const char *text, int width) {
    int indent = strlen(lead);
    int column = 0;
    Serial.print(lead);

    int i = 0;
    while (text[i] != '\0') {
        // Skip spaces, then find where the next word ends
        while (text[i] == ' ') {
            i++;
        }
        int word_start = i;
        while (text[i] != '\0' && text[i] != ' ') {
            i++;
        }
        int word_length = i - word_start;

        if (word_length > 0) {
            if (column > 0 && column + 1 + word_length > width) {
                Serial.println();
                print_repeated(' ', indent);
                column = 0;
            }
            if (column > 0) {
                Serial.print(' ');
                column++;
            }
            for (int k = word_start; k < i; k++) {
                Serial.print(text[k]);
            }
            column += word_length;
        }
    }
    Serial.println();
}

/////////////////////////
//// MINUTE REPORT //////
/////////////////////////

// "================ NODE N1 | hh:mm:ss | STATE ======" in the state's colour
void print_header(struct Node_report report) {
    unsigned long t = report.time_sec;
    char middle[64];
    snprintf(middle, 64, " NODE %s | %02lu:%02lu:%02lu | %s ",
             report.id, t / 3600, (t / 60) % 60, t % 60, state_name(report.state));

    int length = strlen(middle);
    int right = HEADER_MIN_RIGHT;
    if (DISPLAY_WIDTH > HEADER_LEFT + length + HEADER_MIN_RIGHT) {
        right = DISPLAY_WIDTH - HEADER_LEFT - length;
    }

    Serial.print(state_colour(report.state));
    print_repeated('=', HEADER_LEFT);
    Serial.print(middle);
    print_repeated('=', right);
    Serial.println(colour_end());
}

// Table of each sensor's reading, its normal value and the change
void print_readings(struct Node_report report) {
    print_padded("Reading", 12);
    print_padded("Now", 10);
    print_padded("Normal", 10);
    Serial.println("Change");

    for (int s = 0; s < S_COUNT; s++) {
        char now[16];
        char base[16];
        char change[16];
        bool both_valid = report.now_valid[s] && report.base_valid[s];
        format_value(now, 16, s, report.now[s], report.now_valid[s]);
        format_value(base, 16, s, report.base[s], report.base_valid[s]);
        format_change(change, 16, s, report.now[s], report.base[s], both_valid);

        print_padded(sensor_name(s), 12);
        print_padded(now, 10);
        print_padded(base, 10);
        Serial.println(change);
    }
}

// One [FAULT] line for each broken sensor
void print_faults(struct Node_report report) {
    for (int s = 0; s < S_COUNT; s++) {
        if (report.fault[s] != F_NONE) {
            Serial.print(colour(MAGENTA));
            Serial.print("[FAULT] ");
            Serial.print(sensor_name(s));
            Serial.print(": ");
            Serial.print(fault_reason(s, report.fault[s]));
            Serial.println(colour_end());
        }
    }
    if (report.ph_recalibrate) {
        Serial.print(colour(MAGENTA));
        Serial.print("[FAULT] pH: baseline drifted > ");
        print_float(DRIFT_PH_LIMIT, 1);
        Serial.print(" over 3 days - PH_RECALIBRATE");
        Serial.println(colour_end());
    }
}

// A label's name and confidence, the pollutants it might be, and what it means
void print_label(enum Label_id id, float confidence) {
    struct Label_info info = get_label_info(id);
    Serial.print(category_colour(info.category));
    Serial.print(category_tag(info.category));
    Serial.print(" ");
    Serial.print(info.name);
    Serial.print(colour_end());
    if (info.category == CAT_POLLUTION || info.category == CAT_WATCH) {
        Serial.print("  (conf ");
        print_float(confidence, 2);
        Serial.print(")");
    }
    Serial.println();
    print_wrapped("  Possible pollutants: ", info.pollutants, WRAP_TEXT_COLS);
    print_wrapped("  What it means:       ", info.meaning, WRAP_TEXT_COLS);
}

// The other labels that are on as well as the main one
void print_also(struct Node_report report) {
    if (report.num_also == 0) {
        return;
    }
    char list[160] = "";
    for (int i = 0; i < report.num_also; i++) {
        if (i > 0) {
            strncat(list, ", ", 160 - strlen(list) - 1);
        }
        strncat(list, get_label_info(report.also[i]).code, 160 - strlen(list) - 1);
    }
    print_wrapped("  Also flagged:        ", list, WRAP_TEXT_COLS);
}

void print_disclaimer(void) {
    Serial.print("  Note: ");
    Serial.println(DISCLAIMER_LINE1);
    Serial.print("        ");
    Serial.println(DISCLAIMER_LINE2);
}

// Every node from upstream to downstream, then what the comparison found
void print_network(struct Node_report report, struct Network_assessment a,
    struct River_network *network, unsigned long now_min) {
    int local_index = network_find_node(network, report.id);
    unsigned long local_distance = 0;
    if (local_index >= 0) {
        local_distance = network->nodes[local_index].distance_m;
    }

    Serial.println("Network (upstream -> downstream):");
    for (int i = 0; i < network->num_nodes; i++) {
        struct River_node *node = &network->nodes[i];
        char status[48];
        node_status(network, node, now_min, status, 48);

        const char *tag = "";
        if (node->is_local) {
            tag = "  <- this node";
        } else if (node->distance_m == local_distance) {
            tag = "  <- same position";
        }

        const char *status_colour = colour(GREY);
        if (node->has_report && node_is_fresh(node, now_min)) {
            status_colour = state_colour(node->report.state);
        }

        Serial.print("  ");
        print_padded(node->id, 4);
        Serial.print(" ");
        print_right_aligned(node->distance_m, 6);
        Serial.print("m  ");
        print_padded(node->place, 12);
        Serial.print(" ");
        Serial.print(status_colour);
        Serial.print(status);
        Serial.print(colour_end());
        Serial.println(tag);
    }

    char text[160];
    finding_text(a, text, 160);
    if (text[0] != '\0') {
        Serial.print(finding_colour(a.finding));
        print_wrapped("  >> ", text, DISPLAY_WIDTH - 5);
        Serial.print(colour_end());
    }
}

// The full human-readable block for one minute
void print_human(struct Node_report report, struct Network_assessment a,
    struct River_network *network, unsigned long now_min) {
    print_faults(report);
    print_header(report);
    print_readings(report);

    if (report.state == ST_BASELINE_BUILDING) {
        Serial.print("Learning normal levels: ");
        Serial.print((unsigned long)report.learned_minutes);
        Serial.print("/");
        Serial.print((unsigned long)report.learn_needed);
        Serial.println(" min");
    } else if (report.state == ST_NORMAL && report.label == LBL_NONE) {
        Serial.print("Status: ");
        Serial.print(colour(GREEN));
        Serial.print("normal");
        Serial.println(colour_end());
    } else {
        print_repeated('-', DISPLAY_WIDTH);
        Serial.println();
        if (report.state == ST_FAULT) {
            Serial.print(colour(MAGENTA));
            Serial.print("Status: sensor fault (see [FAULT] lines above)");
            Serial.println(colour_end());
        } else {
            print_label(report.label, report.confidence);
            if (report.state == ST_NORMAL) {
                Serial.println("Status: normal (filtered, no alert)");
            }
        }
        print_also(report);

        // Show the note if anything is an alert or a watch
        bool warn = false;
        if (report.state == ST_ALERT || report.state == ST_WATCH) {
            warn = true;
        }
        for (int i = 0; i < report.num_also; i++) {
            if (get_label_info(report.also[i]).category == CAT_WATCH) {
                warn = true;
            }
        }
        if (warn) {
            print_disclaimer();
        }
    }
    print_repeated('=', DISPLAY_WIDTH);
    Serial.println();
    print_network(report, a, network, now_min);
    Serial.println();
}

/////////////////////////
///////// JSON //////////
/////////////////////////

// A number with the given decimals, or null if it isn't valid
void print_json_number(float value, bool valid, int decimals) {
    if (valid) {
        print_float(value, decimals);
    } else {
        Serial.print("null");
    }
}

// "ph":..,"tds":..,"ntu":..,"temp":..
void print_json_values(float values[], bool valid[]) {
    Serial.print("\"ph\":");
    print_json_number(values[S_PH], valid[S_PH], 2);
    Serial.print(",\"tds\":");
    print_json_number(values[S_TDS], valid[S_TDS], 0);
    Serial.print(",\"ntu\":");
    print_json_number(values[S_NTU], valid[S_NTU], 1);
    Serial.print(",\"temp\":");
    print_json_number(values[S_TEMP], valid[S_TEMP], 1);
}

// The whole minute report on one line, for a computer to read
void print_json(struct Node_report report, struct Network_assessment a) {
    char code[40];
    label_code(report, code, 40);

    Serial.print("JSON:{\"node\":\"");
    Serial.print(report.id);
    Serial.print("\",\"pos\":");
    Serial.print(a.local_distance);
    Serial.print(",\"t\":");
    Serial.print(report.time_sec);
    Serial.print(",\"state\":\"");
    Serial.print(state_name(report.state));
    Serial.print("\",\"label\":\"");
    Serial.print(code);
    Serial.print("\",\"conf\":");
    print_float(report.confidence, 2);
    Serial.print(",");

    if (report.num_also > 0) {
        Serial.print("\"also\":[");
        for (int i = 0; i < report.num_also; i++) {
            if (i > 0) {
                Serial.print(",");
            }
            Serial.print("\"");
            Serial.print(get_label_info(report.also[i]).code);
            Serial.print("\"");
        }
        Serial.print("],");
    }

    print_json_values(report.now, report.now_valid);
    Serial.print(",\"base\":{");
    print_json_values(report.base, report.base_valid);

    Serial.print("},\"faults\":[");
    bool first = true;
    for (int s = 0; s < S_COUNT; s++) {
        if (report.fault[s] != F_NONE) {
            if (!first) {
                Serial.print(",");
            }
            Serial.print("\"");
            Serial.print(sensor_code(s));
            Serial.print("_");
            Serial.print(fault_code_text(report.fault[s]));
            Serial.print("\"");
            first = false;
        }
    }
    if (report.ph_recalibrate) {
        if (!first) {
            Serial.print(",");
        }
        Serial.print("\"PH_RECALIBRATE\"");
    }

    Serial.print("],\"net\":{\"finding\":\"");
    Serial.print(finding_code(a.finding));
    Serial.print("\",\"upstream\":");
    if (a.has_upstream) {
        Serial.print("\"");
        Serial.print(a.upstream_id);
        Serial.print("\"");
    } else {
        Serial.print("null");
    }
    Serial.print(",\"siblings\":");
    Serial.print((unsigned long)a.siblings);
    Serial.print(",\"siblings_alerting\":");
    Serial.print((unsigned long)a.siblings_alerting);
    Serial.print(",\"peers\":");
    Serial.print((unsigned long)a.peers);
    Serial.print(",\"rain_peers\":");
    Serial.print((unsigned long)a.rain_peers);
    Serial.println("}}");
}
