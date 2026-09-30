#include "baseline.h"
#include <math.h>
#include <string.h>

////////////////////////
// Function prototypes//
////////////////////////
void update_sensor_median(struct Baseline *baseline, int sensor);
void track_ph_day(struct Baseline *baseline, bool valid, float ph);
void track_ph_drift(struct Baseline *baseline);
bool ph_drift_found(struct Baseline *baseline);
////////////////////////

// Median of values[0..n). This sorts the array (insertion sort, n is small).
float median_of(float values[], int n) {
    if (n == 0) {
        return 0.0f;
    }
    for (int i = 1; i < n; i++) {
        float x = values[i];
        int j = i - 1;
        while (j >= 0 && values[j] > x) {
            values[j + 1] = values[j];
            j--;
        }
        values[j + 1] = x;
    }

    if (n % 2 == 1) {
        return values[n / 2];
    } else {
        return 0.5f * (values[n / 2 - 1] + values[n / 2]);
    }
}

// Forgets everything the baseline has learned
void baseline_reset(struct Baseline *baseline) {
    memset(baseline, 0, sizeof(struct Baseline)); // sets every value in the struct to 0
}

// Called once a minute with that minute's medians.
// add[s] == false freezes sensor s (alert on, rain, or sensor fault).
void baseline_add_minute(struct Baseline *baseline, float values[], bool valid[], bool add[]) {
    bool any = false;
    for (int s = 0; s < S_COUNT; s++) {
        if (valid[s] && add[s]) {
            any = true;
            baseline->pending[s][baseline->num_pending[s]] = values[s];
            baseline->num_pending[s]++;

            // A full slot's worth of minutes: save its median into the ring buffer
            if (baseline->num_pending[s] >= BASELINE_SLOT_MIN) {
                int head = baseline->head[s];
                baseline->slots[s][head] = median_of(baseline->pending[s], baseline->num_pending[s]);
                baseline->num_pending[s] = 0;
                baseline->head[s] = (head + 1) % BASELINE_SLOTS;
                if (baseline->count[s] < BASELINE_SLOTS) {
                    baseline->count[s]++;
                }
                update_sensor_median(baseline, s);
            }
        }
    }
    if (any && baseline->learned_minutes < 0xFFFF) {
        baseline->learned_minutes++;
    }

    track_ph_day(baseline, valid[S_PH], values[S_PH]);
    track_ph_drift(baseline);
}

// true once the warm-up time has been learned
bool baseline_ready(struct Baseline *baseline) {
    return baseline->learned_minutes >= BASELINE_MIN_MINUTES;
}

// true if the sensor has at least one slot of data
bool baseline_has(struct Baseline *baseline, int sensor) {
    return baseline->count[sensor] > 0;
}

// Gets the lowest and highest pH of the last 24 h.
// Returns false until there's NUTRIENT_MIN_MINUTES of data.
bool baseline_ph_day_range(struct Baseline *baseline, float *min_ph, float *max_ph) {
    if (baseline->ph_tracked < NUTRIENT_MIN_MINUTES) {
        return false;
    }
    bool found = false;
    for (int i = 0; i < PH_DAY_BUCKETS; i++) {
        if (baseline->hour_has[i]) {
            if (!found || baseline->hour_min[i] < *min_ph) {
                *min_ph = baseline->hour_min[i];
            }
            if (!found || baseline->hour_max[i] > *max_ph) {
                *max_ph = baseline->hour_max[i];
            }
            found = true;
        }
    }
    return found;
}

/////////////////////////
// Function definitions//
/////////////////////////

// Works out the new baseline for one sensor from its slots
void update_sensor_median(struct Baseline *baseline, int sensor) {
    float copy[BASELINE_SLOTS];   // median_of() sorts, so give it a copy
    for (int i = 0; i < baseline->count[sensor]; i++) {
        copy[i] = baseline->slots[sensor][i];
    }
    baseline->median[sensor] = median_of(copy, baseline->count[sensor]);
}

void track_ph_day(struct Baseline *baseline, bool valid, float ph) {
    int hour = baseline->hour_index;
    if (valid) {
        if (!baseline->hour_has[hour]) {
            baseline->hour_min[hour] = ph;
            baseline->hour_max[hour] = ph;
            baseline->hour_has[hour] = true;
        } else {
            if (ph < baseline->hour_min[hour]) {
                baseline->hour_min[hour] = ph;
            }
            if (ph > baseline->hour_max[hour]) {
                baseline->hour_max[hour] = ph;
            }
        }
    }

    // Move on to the next hour every 60 minutes
    baseline->minute_in_hour++;
    if (baseline->minute_in_hour >= 60) {
        baseline->minute_in_hour = 0;
        baseline->hour_index = (hour + 1) % PH_DAY_BUCKETS;
        baseline->hour_has[baseline->hour_index] = false;
    }
    if (baseline->ph_tracked < 0xFFFFFFFFUL) {
        baseline->ph_tracked++;
    }
}

// Every DRIFT_SNAPSHOT_MIN minutes, save a copy of the baseline so a slowly
// drifting pH probe can be spotted
void track_ph_drift(struct Baseline *baseline) {
    if (DEMO_MODE) {
        return;
    }
    baseline->minutes_since_snapshot++;
    if (baseline->minutes_since_snapshot < DRIFT_SNAPSHOT_MIN) {
        return;
    }
    baseline->minutes_since_snapshot = 0;
    for (int s = 0; s < S_COUNT; s++) {
        if (baseline->count[s] == 0) {
            return;
        }
    }

    // History is full: throw away the oldest one by shifting everything down
    if (baseline->num_drift == DRIFT_SNAPSHOTS) {
        for (int i = 0; i < DRIFT_SNAPSHOTS - 1; i++) {
            for (int s = 0; s < S_COUNT; s++) {
                baseline->drift_history[i][s] = baseline->drift_history[i + 1][s];
            }
        }
        baseline->num_drift--;
    }
    for (int s = 0; s < S_COUNT; s++) {
        baseline->drift_history[baseline->num_drift][s] = baseline->median[s];
    }
    baseline->num_drift++;

    if (baseline->num_drift == DRIFT_SNAPSHOTS && ph_drift_found(baseline)) {
        baseline->drift_detected = true;
    } else {
        baseline->drift_detected = false;
    }
}

// The pH baseline kept moving the same way (always up or always down) by
// more than DRIFT_PH_LIMIT, while the other sensors stayed within
// DRIFT_OTHER_TOL
bool ph_drift_found(struct Baseline *baseline) {
    int direction = 0;
    for (int i = 1; i < baseline->num_drift; i++) {
        float change = baseline->drift_history[i][S_PH] - baseline->drift_history[i - 1][S_PH];
        if (change > 0) {
            if (direction < 0) {
                return false;
            }
            direction = 1;
        } else if (change < 0) {
            if (direction > 0) {
                return false;
            }
            direction = -1;
        }
    }

    float *first = baseline->drift_history[0];
    float *last = baseline->drift_history[baseline->num_drift - 1];
    if (fabsf(last[S_PH] - first[S_PH]) <= DRIFT_PH_LIMIT) {
        return false;
    }
    for (int s = S_TDS; s < S_COUNT; s++) {
        float ref = 1.0f;
        if (fabsf(first[s]) > 1e-3f) {
            ref = fabsf(first[s]);
        }
        if (fabsf(last[s] - first[s]) / ref > DRIFT_OTHER_TOL) {
            return false;
        }
    }
    return true;
}
