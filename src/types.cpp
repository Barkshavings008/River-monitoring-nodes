#include "types.h"

// Human name of a sensor, e.g. "Turbidity"
const char *sensor_name(int sensor) {
    if (sensor == S_PH) {
        return "pH";
    } else if (sensor == S_TDS) {
        return "TDS";
    } else if (sensor == S_NTU) {
        return "Turbidity";
    } else if (sensor == S_TEMP) {
        return "Water temp";
    } else {
        return "?";
    }
}

// Short code of a sensor, e.g. "NTU"
const char *sensor_code(int sensor) {
    if (sensor == S_PH) {
        return "PH";
    } else if (sensor == S_TDS) {
        return "TDS";
    } else if (sensor == S_NTU) {
        return "NTU";
    } else if (sensor == S_TEMP) {
        return "TEMP";
    } else {
        return "?";
    }
}

const char *state_name(enum Node_state state) {
    if (state == ST_BASELINE_BUILDING) {
        return "BASELINE_BUILDING";
    } else if (state == ST_NORMAL) {
        return "NORMAL";
    } else if (state == ST_WATCH) {
        return "WATCH";
    } else if (state == ST_ALERT) {
        return "ALERT";
    } else {
        return "FAULT";
    }
}

// true if the main label or any of the "also" labels is id
bool report_has_label(struct Node_report report, enum Label_id id) {
    if (report.label == id) {
        return true;
    }
    for (int i = 0; i < report.num_also; i++) {
        if (report.also[i] == id) {
            return true;
        }
    }
    return false;
}
