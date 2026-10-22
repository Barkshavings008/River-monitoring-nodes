#include "phone_messages.h"
#include "labels.h"
#include "report_text.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Text being built up. If it ever runs out of room, full gets set and the
// message isn't sent (half a message would just confuse the phone).
struct Text_out {
    char *text;
    int size;
    int length;
    bool full;
};

////////////////////////
// Function prototypes//
////////////////////////
struct Text_out text_start(char *out, int size);
int text_finish(struct Text_out *t);
void add_format(struct Text_out *t, const char *format, ...);
void add_string(struct Text_out *t, const char *text);
void add_number(struct Text_out *t, float value, bool valid, int decimals);
void add_values(struct Text_out *t, float values[], bool valid[]);
void add_label(struct Text_out *t, enum Label_id id);
void add_short_label(struct Text_out *t, enum Label_id id);
void add_speed(struct Text_out *t, struct Speed_estimate estimate, unsigned long now_min);
const char *category_word(enum Category category);
const char *json_bool(bool value);
int fitted_bits(struct Node_report report);
void add_reading_lines(struct Text_out *t, struct Node_report report);
void add_reading(struct Text_out *t, const char *name, float now, bool now_ok, float base,
    bool base_ok, int decimals, const char *unit);
////////////////////////

#define NOTIFY_TEXT_SIZE 1100   // the notification's text, before it's put into JSON

// Priorities in ntfy: 5 = urgent (pops up and sounds), 3 = normal, 1 = silent
#define PRIORITY_POLLUTION 5
#define PRIORITY_WATCH 4
#define PRIORITY_FAULT 4
#define PRIORITY_SPEED 3

// {"type":"status","node":"N1","min":42,...}
int build_status_message(char *out, int size, struct Node_report report,
    struct Network_assessment assessment, struct Speed_tracker *speed, unsigned long now_min,
    const char *wifi_state, bool armed) {
    struct Text_out t = text_start(out, size);
    char code[40];
    char text[FINDING_TEXT_SIZE];
    label_code(report, code, 40);

    add_format(&t, "{\"type\":\"status\",\"node\":");
    add_string(&t, report.id);
    add_format(&t, ",\"pos\":%lu", assessment.local_distance);
    if (assessment.branched) {
        add_format(&t, ",\"branch\":");
        add_string(&t, assessment.local_branch);
    }
    add_format(&t, ",\"min\":%lu,\"minute_s\":%lu,\"sim\":%s", now_min,
               (unsigned long)(MINUTE_MS / 1000), json_bool(SIMULATE));
    add_format(&t, ",\"state\":");
    add_string(&t, state_name(report.state));
    add_format(&t, ",\"code\":");
    add_string(&t, code);
    add_format(&t, ",\"conf\":%.2f,\"label\":", report.confidence);
    add_label(&t, report.label);

    add_format(&t, ",\"also\":[");
    for (int i = 0; i < report.num_also; i++) {
        if (i > 0) {
            add_format(&t, ",");
        }
        add_short_label(&t, report.also[i]);
    }
    add_format(&t, "],\"now\":");
    add_values(&t, report.now, report.now_valid);
    add_format(&t, ",\"base\":");
    add_values(&t, report.base, report.base_valid);
    add_format(&t, ",\"fitted\":{\"ph\":%s,\"tds\":%s,\"ntu\":%s,\"temp\":%s}",
               json_bool(report.fitted[S_PH]), json_bool(report.fitted[S_TDS]),
               json_bool(report.fitted[S_NTU]), json_bool(report.fitted[S_TEMP]));

    add_format(&t, ",\"faults\":[");
    bool first = true;
    for (int s = 0; s < S_COUNT; s++) {
        if (report.fault[s] != F_NONE) {
            if (!first) {
                add_format(&t, ",");
            }
            add_format(&t, "{\"sensor\":");
            add_string(&t, sensor_name(s));
            add_format(&t, ",\"text\":");
            add_string(&t, fault_reason(s, report.fault[s]));
            add_format(&t, "}");
            first = false;
        }
    }
    add_format(&t, "],\"ph_recalibrate\":%s", json_bool(report.ph_recalibrate));
    add_format(&t, ",\"learned_min\":%d,\"learn_needed_min\":%d", report.learned_minutes,
               report.learn_needed);

    finding_text(assessment, text, FINDING_TEXT_SIZE);
    add_format(&t, ",\"finding\":");
    add_string(&t, finding_code(assessment.finding));
    add_format(&t, ",\"finding_text\":");
    add_string(&t, text);

    add_format(&t, ",\"wifi\":");
    add_string(&t, wifi_state);
    add_format(&t, ",\"armed\":%s", json_bool(armed));

    add_format(&t, ",\"speed\":");
    if (speed != NULL && speed->has_estimate) {
        add_speed(&t, speed->latest, now_min);
    } else {
        add_format(&t, "null");
    }
    add_format(&t, "}\n");
    return text_finish(&t);
}

// {"type":"alert","on":true,"label":{...},"now":{...}}
int build_alert_message(char *out, int size, struct Alert_event event, struct Node_report report,
    unsigned long now_min) {
    struct Text_out t = text_start(out, size);
    add_format(&t, "{\"type\":\"alert\",\"node\":");
    add_string(&t, report.id);
    add_format(&t, ",\"min\":%lu,\"on\":%s,\"conf\":%.2f,\"label\":", now_min,
               json_bool(event.on), event.confidence);
    add_label(&t, event.id);
    add_format(&t, ",\"now\":");
    add_values(&t, report.now, report.now_valid);
    add_format(&t, "}\n");
    return text_finish(&t);
}

// {"type":"nodes","nodes":[{"id":"N0",...},...]}
// Nodes with fresh data have "live":true and their state, label and readings.
// The rest have "live":false and say why ("offline" or "no data").
int build_nodes_message(char *out, int size, struct River_network *network, unsigned long now_min) {
    struct Text_out t = text_start(out, size);
    add_format(&t, "{\"type\":\"nodes\",\"min\":%lu,\"nodes\":[", now_min);
    for (int i = 0; i < network->num_nodes; i++) {
        struct River_node *node = &network->nodes[i];
        if (i > 0) {
            add_format(&t, ",");
        }
        add_format(&t, "{\"id\":");
        add_string(&t, node->id);
        add_format(&t, ",\"place\":");
        add_string(&t, node->place);
        add_format(&t, ",\"branch\":");
        add_string(&t, network->branches[node->branch].name);
        add_format(&t, ",\"dist\":%lu,\"local\":%s", node->distance_m, json_bool(node->is_local));

        bool live = node->has_report && (node->is_local || node_is_fresh(node, now_min));
        if (live) {
            struct Label_info info = get_label_info(node->report.label);
            add_format(&t, ",\"live\":true,\"state\":");
            add_string(&t, state_name(node->report.state));
            add_format(&t, ",\"label\":");
            add_string(&t, info.short_name);
            add_format(&t, ",\"category\":");
            add_string(&t, category_word(info.category));
            add_format(&t, ",\"fitted\":%d,\"now\":", fitted_bits(node->report));
            add_values(&t, node->report.now, node->report.now_valid);
        } else {
            char status[48];
            node_status(network, node, now_min, status, 48);
            add_format(&t, ",\"live\":false,\"status\":");
            add_string(&t, status);
        }
        add_format(&t, "}");
    }
    add_format(&t, "]}\n");
    return text_finish(&t);
}

// {"type":"speed","mps":0.83,...}
int build_speed_message(char *out, int size, struct Speed_estimate estimate, unsigned long now_min) {
    struct Text_out t = text_start(out, size);
    add_format(&t, "{\"type\":\"speed\",\"speed\":");
    add_speed(&t, estimate, now_min);
    add_format(&t, "}\n");
    return text_finish(&t);
}

/////////////////////////
//// WIFI NOTIFICATIONS ////
/////////////////////////

// {"topic":"...","title":"...","message":"...","priority":5,"tags":["rotating_light"]}
// The message can have line breaks in it. Returns the length, or 0 if it
// didn't fit.
int build_ntfy(char *out, int size, const char *topic, const char *title, const char *message,
    int priority, const char *tag) {
    struct Text_out t = text_start(out, size);
    add_format(&t, "{\"topic\":");
    add_string(&t, topic);
    add_format(&t, ",\"title\":");
    add_string(&t, title);
    add_format(&t, ",\"message\":");
    add_string(&t, message);
    add_format(&t, ",\"priority\":%d,\"tags\":[", priority);
    add_string(&t, tag);
    add_format(&t, "]}");
    return text_finish(&t);
}

// One pop-up for every pollution/watch label that turned on this minute:
//
//   POLLUTION ALERT at N1
//   Likely heavy metals / acid mine drainage (77% sure)
//   Also: Likely industrial discharge (67% sure)
//
//   pH 4.60 (normal 7.20)
//   ...
//   Possible pollutants: ...
//   Source likely between N0 (0 m) and N1 (1000 m) ...
//   Water speed: about 0.61 m/s (N0 to N1, 12 min ago)
//
// Returns 0 if no pollution or watch label turned on.
int build_ntfy_alert(char *out, int size, const char *topic, struct Alert_event events[],
    int num_events, struct Node_report report, struct Network_assessment assessment,
    struct Speed_tracker *speed, unsigned long now_min) {
    char text[NOTIFY_TEXT_SIZE];
    struct Text_out t = text_start(text, NOTIFY_TEXT_SIZE);
    int found = 0;
    bool pollution = false;
    enum Label_id first = LBL_NONE;

    // Pollution labels first, then watch labels
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < num_events; i++) {
            struct Label_info info = get_label_info(events[i].id);
            bool wanted = (pass == 0 && info.category == CAT_POLLUTION) ||
                          (pass == 1 && info.category == CAT_WATCH);
            if (!events[i].on || !wanted) {
                continue;
            }
            if (found == 0) {
                first = events[i].id;
            } else {
                add_format(&t, "Also: ");
            }
            add_format(&t, "%s (%d%% sure)\n", info.name, (int)(events[i].confidence * 100.0f + 0.5f));
            if (info.category == CAT_POLLUTION) {
                pollution = true;
            }
            found++;
        }
    }
    if (found == 0) {
        return 0;
    }

    add_format(&t, "\n");
    add_reading_lines(&t, report);
    struct Label_info main_info = get_label_info(first);
    if (main_info.pollutants[0] != '\0') {
        add_format(&t, "\nPossible pollutants: %s\n", main_info.pollutants);
    }
    char where[FINDING_TEXT_SIZE];
    finding_text(assessment, where, FINDING_TEXT_SIZE);
    if (where[0] != '\0') {
        add_format(&t, "%s\n", where);
    }
    if (speed != NULL && speed->has_estimate) {
        char ago[16];
        format_duration(ago, 16, now_min - speed->latest.found_min);
        add_format(&t, "Water speed: about %.2f m/s (%s to %s, %s ago)\n",
                   speed->latest.metres_per_sec, speed->latest.from_id, speed->latest.to_id, ago);
    }
    if (SIMULATE) {
        add_format(&t, "(Simulated readings)\n");
    }
    add_format(&t, "Confirm with a lab water sample.");
    if (text_finish(&t) == 0) {
        return 0;
    }

    char title[64];
    int priority = PRIORITY_WATCH;
    const char *tag = "warning";
    if (pollution) {
        snprintf(title, 64, "POLLUTION ALERT at %s", report.id);
        priority = PRIORITY_POLLUTION;
        tag = "rotating_light";
    } else {
        snprintf(title, 64, "Watch at %s", report.id);
    }
    return build_ntfy(out, size, topic, title, text, priority, tag);
}

// One pop-up for the new water speed(s) worked out this minute
int build_ntfy_speed(char *out, int size, const char *topic, struct Speed_estimate speeds[],
    int num_speeds) {
    if (num_speeds <= 0) {
        return 0;
    }
    char text[NOTIFY_TEXT_SIZE];
    struct Text_out t = text_start(text, NOTIFY_TEXT_SIZE);
    for (int i = 0; i < num_speeds; i++) {
        // The same pair of nodes can come up for two labels at once: only say it once
        bool repeat = false;
        for (int k = 0; k < i; k++) {
            if (strcmp(speeds[k].from_id, speeds[i].from_id) == 0 &&
                strcmp(speeds[k].to_id, speeds[i].to_id) == 0 && speeds[k].minutes == speeds[i].minutes) {
                repeat = true;
            }
        }
        if (repeat) {
            continue;
        }
        char took[16];
        format_duration(took, 16, speeds[i].minutes);
        add_format(&t, "%s went from %s to %s: %lu m in %s, about %.2f m/s (%.1f km/h).\n",
                   get_label_info(speeds[i].label).short_name, speeds[i].from_id, speeds[i].to_id,
                   speeds[i].metres, took, speeds[i].metres_per_sec, speeds[i].metres_per_sec * 3.6f);
    }
    add_format(&t, "Worked out from the pollution reaching one node and then the next.");
    if (text_finish(&t) == 0) {
        return 0;
    }
    char title[64];
    snprintf(title, 64, "Water speed: about %.2f m/s", speeds[0].metres_per_sec);
    return build_ntfy(out, size, topic, title, text, PRIORITY_SPEED, "ocean");
}

// A pop-up for a broken sensor (sensor = S_COUNT means the pH probe needs recalibrating)
int build_ntfy_fault(char *out, int size, const char *topic, struct Node_report report, int sensor) {
    char title[64];
    char text[NOTIFY_TEXT_SIZE];
    if (sensor >= 0 && sensor < S_COUNT) {
        snprintf(title, 64, "Sensor problem at %s: %s", report.id, sensor_name(sensor));
        snprintf(text, NOTIFY_TEXT_SIZE, "%s sensor: %s.\nIts readings are left out until it's fixed, "
                 "so it can't cause a false alert.", sensor_name(sensor),
                 fault_reason(sensor, report.fault[sensor]));
    } else {
        snprintf(title, 64, "Sensor problem at %s: pH probe drifting", report.id);
        snprintf(text, NOTIFY_TEXT_SIZE, "The pH reading has drifted steadily for 3 days while the "
                 "other sensors stayed put.\nRecalibrate the pH probe in pH 7 and pH 4 buffers.");
    }
    return build_ntfy(out, size, topic, title, text, PRIORITY_FAULT, "wrench");
}

// A pop-up with the intro line and the current readings (used for "online",
// "test" and "armed" notifications)
int build_ntfy_readings(char *out, int size, const char *topic, const char *title,
    const char *intro, struct Node_report report, int priority, const char *tag) {
    char text[NOTIFY_TEXT_SIZE];
    struct Text_out t = text_start(text, NOTIFY_TEXT_SIZE);
    add_format(&t, "%s\n", intro);
    if (report.state != ST_BASELINE_BUILDING || report.now_valid[S_PH]) {
        add_format(&t, "\n");
        add_reading_lines(&t, report);
    }
    if (text_finish(&t) == 0) {
        return 0;
    }
    // No blank line at the end
    while (t.length > 0 && text[t.length - 1] == '\n') {
        t.length--;
        text[t.length] = '\0';
    }
    return build_ntfy(out, size, topic, title, text, priority, tag);
}

/////////////////////////
// Function definitions//
/////////////////////////

// pH 4.60 (normal 7.20)
// TDS 617 mg/L (normal 210)
// Turbidity 18.2 NTU (normal 8.2)
// Water temp 17.4 °C (normal 17.1)
// (sensors this node doesn't have are left out)
void add_reading_lines(struct Text_out *t, struct Node_report report) {
    if (report.fitted[S_PH]) {
        add_reading(t, "pH", report.now[S_PH], report.now_valid[S_PH], report.base[S_PH],
                    report.base_valid[S_PH], 2, "");
    }
    if (report.fitted[S_TDS]) {
        add_reading(t, "TDS", report.now[S_TDS], report.now_valid[S_TDS], report.base[S_TDS],
                    report.base_valid[S_TDS], 0, " mg/L");
    }
    if (report.fitted[S_NTU]) {
        add_reading(t, "Turbidity", report.now[S_NTU], report.now_valid[S_NTU], report.base[S_NTU],
                    report.base_valid[S_NTU], 1, " NTU");
    }
    if (report.fitted[S_TEMP]) {
        add_reading(t, "Water temp", report.now[S_TEMP], report.now_valid[S_TEMP], report.base[S_TEMP],
                    report.base_valid[S_TEMP], 1, " \xC2\xB0" "C");
    }
}

void add_reading(struct Text_out *t, const char *name, float now, bool now_ok, float base,
    bool base_ok, int decimals, const char *unit) {
    if (!now_ok || isnan(now)) {
        add_format(t, "%s: no reading\n", name);
        return;
    }
    add_format(t, "%s %.*f%s", name, decimals, now, unit);
    if (base_ok && !isnan(base)) {
        add_format(t, " (normal %.*f)", decimals, base);
    }
    add_format(t, "\n");
}

struct Text_out text_start(char *out, int size) {
    struct Text_out t;
    t.text = out;
    t.size = size;
    t.length = 0;
    t.full = false;
    if (size > 0) {
        out[0] = '\0';
    }
    return t;
}

// Returns the message's length, or 0 (and an empty string) if it didn't fit
int text_finish(struct Text_out *t) {
    if (t->full) {
        if (t->size > 0) {
            t->text[0] = '\0';
        }
        return 0;
    }
    return t->length;
}

// Adds text the same way printf would
void add_format(struct Text_out *t, const char *format, ...) {
    if (t->full) {
        return;
    }
    va_list args;
    va_start(args, format);
    int room = t->size - t->length;
    int written = vsnprintf(t->text + t->length, room, format, args);
    va_end(args);
    if (written < 0 || written >= room) {
        t->full = true;
        return;
    }
    t->length += written;
}

// Adds "text" in quotes, with any quotes, backslashes or control characters
// inside it escaped so the JSON stays valid
void add_string(struct Text_out *t, const char *text) {
    add_format(t, "\"");
    if (text != NULL) {
        for (int i = 0; text[i] != '\0'; i++) {
            char c = text[i];
            if (c == '"' || c == '\\') {
                add_format(t, "\\%c", c);
            } else if ((unsigned char)c < 0x20) {
                add_format(t, "\\u%04x", (unsigned)(unsigned char)c);
            } else {
                add_format(t, "%c", c);
            }
        }
    }
    add_format(t, "\"");
}

// A number, or null if there's no reading
void add_number(struct Text_out *t, float value, bool valid, int decimals) {
    if (valid && !isnan(value) && !isinf(value)) {
        add_format(t, "%.*f", decimals, value);
    } else {
        add_format(t, "null");
    }
}

// {"ph":7.21,"tds":210,"ntu":8.1,"temp":17.1}
void add_values(struct Text_out *t, float values[], bool valid[]) {
    add_format(t, "{\"ph\":");
    add_number(t, values[S_PH], valid[S_PH], 2);
    add_format(t, ",\"tds\":");
    add_number(t, values[S_TDS], valid[S_TDS], 0);
    add_format(t, ",\"ntu\":");
    add_number(t, values[S_NTU], valid[S_NTU], 1);
    add_format(t, ",\"temp\":");
    add_number(t, values[S_TEMP], valid[S_TEMP], 1);
    add_format(t, "}");
}

// {"code":"LIKELY_SEWAGE","name":"Likely sewage","category":"POLLUTION",...}
void add_label(struct Text_out *t, enum Label_id id) {
    struct Label_info info = get_label_info(id);
    add_format(t, "{\"code\":");
    add_string(t, info.code);
    add_format(t, ",\"name\":");
    add_string(t, info.name);
    add_format(t, ",\"category\":");
    add_string(t, category_word(info.category));
    add_format(t, ",\"pollutants\":");
    add_string(t, info.pollutants);
    add_format(t, ",\"meaning\":");
    add_string(t, info.meaning);
    add_format(t, "}");
}

// {"code":"LIKELY_SEWAGE","name":"Likely sewage","category":"POLLUTION"}
// (for the "also" list and the node list, where there isn't room for more)
void add_short_label(struct Text_out *t, enum Label_id id) {
    struct Label_info info = get_label_info(id);
    add_format(t, "{\"code\":");
    add_string(t, info.code);
    add_format(t, ",\"name\":");
    add_string(t, info.short_name);
    add_format(t, ",\"category\":");
    add_string(t, category_word(info.category));
    add_format(t, "}");
}

// {"mps":0.83,"from":"N0","to":"N1","metres":1000,"minutes":20,"age_min":3,"label":"Likely sewage"}
void add_speed(struct Text_out *t, struct Speed_estimate estimate, unsigned long now_min) {
    add_format(t, "{\"mps\":%.2f,\"from\":", estimate.metres_per_sec);
    add_string(t, estimate.from_id);
    add_format(t, ",\"to\":");
    add_string(t, estimate.to_id);
    unsigned long minute_s = MINUTE_MS / 1000;
    add_format(t, ",\"metres\":%lu,\"minutes\":%lu,\"age_min\":%lu,\"seconds\":%lu,\"age_s\":%lu,\"label\":",
               estimate.metres, estimate.minutes, now_min - estimate.found_min,
               estimate.minutes * minute_s, (now_min - estimate.found_min) * minute_s);
    add_string(t, get_label_info(estimate.label).name);
    add_format(t, "}");
}

const char *category_word(enum Category category) {
    if (category == CAT_POLLUTION) {
        return "POLLUTION";
    } else if (category == CAT_WATCH) {
        return "WATCH";
    } else if (category == CAT_FILTER) {
        return "FILTER";
    } else if (category == CAT_FAULT) {
        return "FAULT";
    } else {
        return "NONE";
    }
}

// Bit s set = the node has sensor s (pH 1, TDS 2, turbidity 4, temp 8)
int fitted_bits(struct Node_report report) {
    int bits = 0;
    for (int s = 0; s < S_COUNT; s++) {
        if (report.fitted[s]) {
            bits |= 1 << s;
        }
    }
    return bits;
}

const char *json_bool(bool value) {
    if (value) {
        return "true";
    } else {
        return "false";
    }
}
