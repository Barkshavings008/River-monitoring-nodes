#include "phone_messages.h"
#include "labels.h"
#include "report_text.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

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
////////////////////////

// {"type":"status","node":"N1","min":42,...}
int build_status_message(char *out, int size, struct Node_report report,
    struct Network_assessment assessment, struct Speed_tracker *speed, unsigned long now_min) {
    struct Text_out t = text_start(out, size);
    char code[40];
    char text[260];
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

    finding_text(assessment, text, 260);
    add_format(&t, ",\"finding\":");
    add_string(&t, finding_code(assessment.finding));
    add_format(&t, ",\"finding_text\":");
    add_string(&t, text);

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
            add_format(&t, ",\"now\":");
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
// Function definitions//
/////////////////////////

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
    add_format(t, ",\"metres\":%lu,\"minutes\":%lu,\"age_min\":%lu,\"label\":", estimate.metres,
               estimate.minutes, now_min - estimate.found_min);
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

const char *json_bool(bool value) {
    if (value) {
        return "true";
    } else {
        return "false";
    }
}
