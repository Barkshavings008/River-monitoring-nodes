#pragma once
// The enums and structs that every part of the program shares.
// (No Arduino stuff in here so it also builds for the PC tests)
#include <stdint.h>
#include "config.h"

enum Sensor {
    S_PH,
    S_TDS,
    S_NTU,
    S_TEMP,
    S_COUNT,   // number of sensors
};

enum Label_id {
    LBL_NONE,
    // C: pollution
    LBL_HEAVY_METALS,
    LBL_INDUSTRIAL,
    LBL_ALKALINE,
    LBL_SEWAGE,
    // D: watch
    LBL_NUTRIENTS,
    LBL_THERMAL,
    LBL_SEDIMENT,
    LBL_SALT,
    LBL_EFFLUENT,
    // B: filter
    LBL_RAIN,
    LBL_FAULT,
    LBL_COUNT,   // number of labels
};

enum Category {
    CAT_NONE,
    CAT_POLLUTION,
    CAT_WATCH,
    CAT_FILTER,
    CAT_FAULT,
};

enum Fault_code {
    F_NONE,
    F_NO_READING,
    F_OUT_OF_RANGE,
    F_FLATLINE,
    F_TEMP_JUMP,
};

enum Node_state {
    ST_BASELINE_BUILDING,
    ST_NORMAL,
    ST_WATCH,
    ST_ALERT,
    ST_FAULT,
};

// One reading from every sensor
struct Reading {
    float value[S_COUNT];
    bool valid[S_COUNT];
};

// What a rule found: which label, and how sure it is (0 to 1)
struct Rule_result {
    enum Label_id id;
    float confidence;
};

#define MAX_ALSO 10  // worst case: 3 other pollution labels + 5 watch labels + rain

// One node's summary of the last minute. This is what gets printed, and what
// a node would send to the other nodes over LoRa/WiFi.
struct Node_report {
    char id[NODE_ID_LEN];
    unsigned long time_sec;       // seconds since the node turned on
    enum Node_state state;
    enum Label_id label;
    float confidence;
    enum Label_id also[MAX_ALSO]; // other labels that are on as well
    int num_also;
    float now[S_COUNT];
    bool now_valid[S_COUNT];
    float base[S_COUNT];
    bool base_valid[S_COUNT];
    enum Fault_code fault[S_COUNT];
    bool ph_recalibrate;
    int learned_minutes;
    int learn_needed;
    bool rain_pattern;            // rain pattern this minute (before persistence)
};

////////////////////////
// Function prototypes//
////////////////////////
const char *sensor_name(int sensor);
const char *sensor_code(int sensor);
const char *state_name(enum Node_state state);
bool report_has_label(struct Node_report report, enum Label_id id);
////////////////////////
