#pragma once
// Sensor fault checks and the pollution pattern rules. No Serial or Arduino
// stuff in here so it can be tested on a PC with fake data.
#include "types.h"

// Everything the rules look at for one minute
struct Rule_inputs {
    float now[S_COUNT];     // this minute's medians
    float base[S_COUNT];    // the baseline ("normal") values
    bool ok[S_COUNT];       // reading is valid, not faulty, and has a baseline
    bool has_tds_step_ref;
    float tds_step_ref;     // TDS up to STEP_WINDOW_MIN ago (for the industrial step change)
    bool has_day_range;     // >= 12 h of pH data and not in demo mode
    float ph_day_min;
    float ph_day_max;
};

#define MAX_RULE_HITS 10

struct Rule_output {
    struct Rule_result hits[MAX_RULE_HITS]; // every label whose required conditions are met
    int num_hits;
    bool rain;                              // B fired, so the C rules and sediment were skipped
};

// Section 4 A: remembers each sensor's history between minutes
struct Fault_tracker {
    float ref[S_COUNT];
    int run[S_COUNT];
    bool has_ref[S_COUNT];
    float prev_temp;
    bool has_prev_temp;
};

////////////////////////
// Function prototypes//
////////////////////////
struct Rule_output evaluate_rules(struct Rule_inputs in);
bool is_rain_event(struct Rule_inputs in);
struct Rule_result rule_heavy_metals(struct Rule_inputs in);
struct Rule_result rule_industrial(struct Rule_inputs in);
struct Rule_result rule_alkaline(struct Rule_inputs in);
struct Rule_result rule_sewage(struct Rule_inputs in);
struct Rule_result rule_nutrients(struct Rule_inputs in);
struct Rule_result rule_thermal(struct Rule_inputs in);
struct Rule_result rule_sediment(struct Rule_inputs in);
struct Rule_result rule_salt(struct Rule_inputs in);
struct Rule_result rule_effluent(struct Rule_inputs in);

void fault_tracker_reset(struct Fault_tracker *tracker);
enum Fault_code check_range(int sensor, float value);
void check_faults(struct Fault_tracker *tracker, float values[], bool valid[], enum Fault_code faults[]);
////////////////////////
