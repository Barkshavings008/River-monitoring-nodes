#include "baseline.h"
#include <math.h>
#include <string.h>

// Median of v[0..n). Uses insertion sort (n is small), so v gets sorted.
float medianOf(float *v, uint16_t n) {
    if (n == 0) {
        return 0.0f;
    }
    for (uint16_t i = 1; i < n; i++) {
        float x = v[i];
        int16_t j = (int16_t)i - 1;
        while (j >= 0 && v[j] > x) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = x;
    }

    if (n % 2 == 1) {
        return v[n / 2];
    }
    return 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

// Clears everything the baseline has learned.
void Baseline::reset() {
    memset(slots_, 0, sizeof(slots_));
    memset(count_, 0, sizeof(count_));
    memset(head_, 0, sizeof(head_));
    memset(pending_, 0, sizeof(pending_));
    memset(pendingN_, 0, sizeof(pendingN_));
    memset(median_, 0, sizeof(median_));
    learned_ = 0;
    memset(hourMin_, 0, sizeof(hourMin_));
    memset(hourMax_, 0, sizeof(hourMax_));
    memset(hourHas_, 0, sizeof(hourHas_));
    hourIdx_ = 0;
    minuteInHour_ = 0;
    phTracked_ = 0;
    memset(driftHist_, 0, sizeof(driftHist_));
    driftN_ = 0;
    minutesSinceSnap_ = 0;
    drift_ = false;
}

// Adds one minute of medians. Once a slot's worth of minutes is collected
// for a sensor, the slot median is pushed into that sensor's ring buffer.
void Baseline::addMinute(const float v[S_COUNT], const bool valid[S_COUNT],
                         const bool add[S_COUNT]) {
    bool any = false;
    for (uint8_t s = 0; s < S_COUNT; s++) {
        if (valid[s] && add[s]) {
            any = true;
            pending_[s][pendingN_[s]] = v[s];
            pendingN_[s]++;

            if (pendingN_[s] >= BASELINE_SLOT_MIN) {
                slots_[s][head_[s]] = medianOf(pending_[s], pendingN_[s]);
                pendingN_[s] = 0;
                head_[s] = (head_[s] + 1) % BASELINE_SLOTS;
                if (count_[s] < BASELINE_SLOTS) {
                    count_[s]++;
                }
                recompute(s);
            }
        }
    }
    if (any && learned_ < 0xFFFF) {
        learned_++;
    }

    trackPhDay(valid[S_PH], v[S_PH]);
    trackDrift();
}

// Recalculates the baseline median for sensor s from its slots.
void Baseline::recompute(uint8_t s) {
    static float scratch[BASELINE_SLOTS];
    memcpy(scratch, slots_[s], count_[s] * sizeof(float));
    median_[s] = medianOf(scratch, count_[s]);
}

// Keeps the lowest and highest pH seen in each hour of the last day.
void Baseline::trackPhDay(bool valid, float ph) {
    if (valid) {
        if (!hourHas_[hourIdx_]) {
            hourMin_[hourIdx_] = ph;
            hourMax_[hourIdx_] = ph;
            hourHas_[hourIdx_] = true;
        } else {
            if (ph < hourMin_[hourIdx_]) {
                hourMin_[hourIdx_] = ph;
            }
            if (ph > hourMax_[hourIdx_]) {
                hourMax_[hourIdx_] = ph;
            }
        }
    }

    minuteInHour_++;
    if (minuteInHour_ >= 60) {
        minuteInHour_ = 0;
        hourIdx_ = (hourIdx_ + 1) % PH_DAY_BUCKETS;
        hourHas_[hourIdx_] = false;
    }
    if (phTracked_ < 0xFFFFFFFFu) {
        phTracked_++;
    }
}

// Writes the day's lowest pH into mn and highest into mx.
// Returns false if there is not enough data yet.
bool Baseline::phDayRange(float &mn, float &mx) const {
    if (phTracked_ < NUTRIENT_MIN_MINUTES) {
        return false;
    }
    bool found = false;
    for (uint8_t i = 0; i < PH_DAY_BUCKETS; i++) {
        if (hourHas_[i]) {
            if (!found || hourMin_[i] < mn) {
                mn = hourMin_[i];
            }
            if (!found || hourMax_[i] > mx) {
                mx = hourMax_[i];
            }
            found = true;
        }
    }
    return found;
}

// Every DRIFT_SNAPSHOT_MIN minutes, saves a copy of the baseline so slow
// pH probe drift can be spotted.
void Baseline::trackDrift() {
    if (DEMO_MODE) {
        return;
    }
    minutesSinceSnap_++;
    if (minutesSinceSnap_ < DRIFT_SNAPSHOT_MIN) {
        return;
    }
    minutesSinceSnap_ = 0;
    for (uint8_t s = 0; s < S_COUNT; s++) {
        if (count_[s] == 0) {
            return;
        }
    }

    if (driftN_ == DRIFT_SNAPSHOTS) {
        memmove(driftHist_[0], driftHist_[1],
                (DRIFT_SNAPSHOTS - 1) * sizeof(driftHist_[0]));
        driftN_--;
    }
    for (uint8_t s = 0; s < S_COUNT; s++) {
        driftHist_[driftN_][s] = median_[s];
    }
    driftN_++;

    if (driftN_ == DRIFT_SNAPSHOTS && checkDrift()) {
        drift_ = true;
    } else {
        drift_ = false;
    }
}

// pH baseline moved steadily (monotonic) by > DRIFT_PH_LIMIT over the
// history, while the other sensors' baselines stayed within
// DRIFT_OTHER_TOL.
bool Baseline::checkDrift() const {
    int dir = 0;
    for (uint8_t i = 1; i < driftN_; i++) {
        float d = driftHist_[i][S_PH] - driftHist_[i - 1][S_PH];
        if (d > 0) {
            if (dir < 0) {
                return false;
            }
            dir = 1;
        } else if (d < 0) {
            if (dir > 0) {
                return false;
            }
            dir = -1;
        }
    }

    const float *first = driftHist_[0];
    const float *last = driftHist_[driftN_ - 1];
    if (fabsf(last[S_PH] - first[S_PH]) <= DRIFT_PH_LIMIT) {
        return false;
    }
    for (uint8_t s = S_TDS; s < S_COUNT; s++) {
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
