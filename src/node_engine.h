#pragma once
// One node's whole pipeline: 2 s samples -> 1 min medians -> faults -> rules
// -> persistence -> baseline update -> Node_report. No Arduino stuff in here,
// so the same code runs the real node, the simulated nodes and the PC tests.
#include "types.h"
#include "baseline.h"
#include "rules.h"
#include "persistence.h"

struct Node_engine {
    char id[NODE_ID_LEN];
    float samples[S_COUNT][MAX_SAMPLES_PER_MIN];  // this minute's samples
    int num_samples[S_COUNT];
    struct Baseline baseline;
    struct Fault_tracker faults;
    struct Persistence persistence;
    float tds_history[STEP_WINDOW_MIN];           // 1 min TDS values for the step change check
    int num_tds_history;
    int hold_minutes_left;                        // baseline stays frozen until this reaches 0
    int tds_history_head;
};

////////////////////////
// Function prototypes//
////////////////////////
void engine_begin(struct Node_engine *engine, const char *id);
void engine_add_sample(struct Node_engine *engine, struct Reading reading);
int engine_close_minute(struct Node_engine *engine, unsigned long uptime_sec,
    struct Node_report *report, struct Alert_event events[], int max_events);
////////////////////////
