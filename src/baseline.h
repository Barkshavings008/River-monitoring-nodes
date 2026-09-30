#pragma once
// The rolling "normal" for this spot in the river. Every sensor has a ring
// buffer of slot medians, and the baseline is the median of those.
#include "types.h"

struct Baseline {
    float slots[S_COUNT][BASELINE_SLOTS];      // ring buffer of slot medians for each sensor
    int count[S_COUNT];                        // how many slots are filled
    int head[S_COUNT];                         // where the next slot goes
    float pending[S_COUNT][BASELINE_SLOT_MIN]; // minutes waiting to make up the next slot
    int num_pending[S_COUNT];
    float median[S_COUNT];                     // the baseline itself
    int learned_minutes;

    // Lowest and highest pH seen in each hour of the last day
    float hour_min[PH_DAY_BUCKETS];
    float hour_max[PH_DAY_BUCKETS];
    bool hour_has[PH_DAY_BUCKETS];
    int hour_index;
    int minute_in_hour;
    unsigned long ph_tracked;

    // Copies of the baseline every 6 h, to spot the pH probe drifting
    float drift_history[DRIFT_SNAPSHOTS][S_COUNT];
    int num_drift;
    int minutes_since_snapshot;
    bool drift_detected;
};

////////////////////////
// Function prototypes//
////////////////////////
float median_of(float values[], int n);
void baseline_reset(struct Baseline *baseline);
void baseline_add_minute(struct Baseline *baseline, float values[], bool valid[], bool add[]);
bool baseline_ready(struct Baseline *baseline);
bool baseline_has(struct Baseline *baseline, int sensor);
bool baseline_ph_day_range(struct Baseline *baseline, float *min_ph, float *max_ph);
////////////////////////
