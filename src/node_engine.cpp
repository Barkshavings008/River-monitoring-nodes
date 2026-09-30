#include "node_engine.h"
#include "labels.h"
#include <string.h>

////////////////////////
// Function prototypes//
////////////////////////
void build_report(struct Node_engine *engine, unsigned long uptime_sec, struct Rule_inputs in,
    enum Fault_code faults[], bool valid[], bool rain_pattern, struct Node_report *report);
void sort_active_labels(struct Node_engine *engine, enum Label_id pollution[], int *num_pollution,
    enum Label_id watch[], int *num_watch);
void insert_by_confidence(struct Persistence *persistence, enum Label_id list[], int *n,
    enum Label_id id);
////////////////////////

void engine_begin(struct Node_engine *engine, const char *id) {
    strncpy(engine->id, id, NODE_ID_LEN - 1);
    engine->id[NODE_ID_LEN - 1] = '\0';
    for (int s = 0; s < S_COUNT; s++) {
        engine->num_samples[s] = 0;
    }
    baseline_reset(&engine->baseline);
    fault_tracker_reset(&engine->faults);
    persistence_reset(&engine->persistence);
    engine->num_tds_history = 0;
    engine->tds_history_head = 0;
}

// Saves one sample. Bad readings, and samples past the limit, are dropped.
void engine_add_sample(struct Node_engine *engine, struct Reading reading) {
    for (int s = 0; s < S_COUNT; s++) {
        if (reading.valid[s] && engine->num_samples[s] < MAX_SAMPLES_PER_MIN) {
            engine->samples[s][engine->num_samples[s]] = reading.value[s];
            engine->num_samples[s]++;
        }
    }
}

// Finishes off the current minute and fills in the report. Any label that
// turns on or off goes into events. Returns how many events there were.
int engine_close_minute(struct Node_engine *engine, unsigned long uptime_sec,
    struct Node_report *report, struct Alert_event events[], int max_events) {
    int num_events = 0;

    // 1 minute medians (the median ignores one-off spikes)
    float now[S_COUNT];
    bool valid[S_COUNT];
    for (int s = 0; s < S_COUNT; s++) {
        if (engine->num_samples[s] > 0) {
            valid[s] = true;
            now[s] = median_of(engine->samples[s], engine->num_samples[s]);
        } else {
            valid[s] = false;
            now[s] = 0.0f;
        }
        engine->num_samples[s] = 0;
    }

    enum Fault_code faults[S_COUNT];
    check_faults(&engine->faults, now, valid, faults);

    struct Rule_inputs in;
    for (int s = 0; s < S_COUNT; s++) {
        in.now[s] = now[s];
        in.base[s] = engine->baseline.median[s];
        in.ok[s] = valid[s] && faults[s] == F_NONE && baseline_has(&engine->baseline, s);
    }

    // Oldest TDS value in the step change window
    if (engine->num_tds_history == 0) {
        in.has_tds_step_ref = false;
        in.tds_step_ref = 0.0f;
    } else if (engine->num_tds_history < STEP_WINDOW_MIN) {
        in.has_tds_step_ref = true;
        in.tds_step_ref = engine->tds_history[0];
    } else {
        in.has_tds_step_ref = true;
        in.tds_step_ref = engine->tds_history[engine->tds_history_head];
    }

    in.ph_day_min = 0.0f;
    in.ph_day_max = 0.0f;
    if (DEMO_MODE) {
        in.has_day_range = false;
    } else {
        in.has_day_range = baseline_ph_day_range(&engine->baseline, &in.ph_day_min, &in.ph_day_max);
    }

    if (valid[S_TDS] && faults[S_TDS] == F_NONE) {
        engine->tds_history[engine->tds_history_head] = now[S_TDS];
        engine->tds_history_head = (engine->tds_history_head + 1) % STEP_WINDOW_MIN;
        if (engine->num_tds_history < STEP_WINDOW_MIN) {
            engine->num_tds_history++;
        }
    }

    struct Rule_output out;
    out.num_hits = 0;
    out.rain = false;
    if (baseline_ready(&engine->baseline)) {
        out = evaluate_rules(in);
        num_events = persistence_update(&engine->persistence, out.hits, out.num_hits,
                                        events, max_events);
    }

    // Keep pollution (and rain) out of "normal": don't learn while any pattern
    // is seen or any label is on. Faulty sensors are never learned.
    bool frozen = out.num_hits > 0 || persistence_any_active(&engine->persistence);
    bool add[S_COUNT];
    for (int s = 0; s < S_COUNT; s++) {
        add[s] = valid[s] && faults[s] == F_NONE && !frozen;
    }

    build_report(engine, uptime_sec, in, faults, valid, out.rain, report);
    baseline_add_minute(&engine->baseline, now, valid, add);
    return num_events;
}

/////////////////////////
// Function definitions//
/////////////////////////

void build_report(struct Node_engine *engine, unsigned long uptime_sec, struct Rule_inputs in,
    enum Fault_code faults[], bool valid[], bool rain_pattern, struct Node_report *report) {
    memset(report, 0, sizeof(struct Node_report)); // sets every value in the struct to 0
    strncpy(report->id, engine->id, NODE_ID_LEN - 1);
    report->time_sec = uptime_sec;

    bool any_fault = engine->baseline.drift_detected;
    for (int s = 0; s < S_COUNT; s++) {
        report->now[s] = in.now[s];
        report->now_valid[s] = valid[s];
        report->base[s] = in.base[s];
        report->base_valid[s] = baseline_has(&engine->baseline, s);
        report->fault[s] = faults[s];
        if (faults[s] != F_NONE) {
            any_fault = true;
        }
    }
    report->ph_recalibrate = engine->baseline.drift_detected;
    report->learned_minutes = engine->baseline.learned_minutes;
    report->learn_needed = BASELINE_MIN_MINUTES;
    report->rain_pattern = rain_pattern;

    if (!baseline_ready(&engine->baseline)) {
        report->state = ST_BASELINE_BUILDING;
        report->label = LBL_NONE;
        return;
    }

    // Labels that are on, split into pollution and watch lists
    enum Label_id pollution[4];
    enum Label_id watch[4];
    int num_pollution = 0;
    int num_watch = 0;
    sort_active_labels(engine, pollution, &num_pollution, watch, &num_watch);
    bool rain = engine->persistence.active[LBL_RAIN];

    // Pollution beats a sensor fault, which beats a watch label
    int num_also = 0;
    if (num_pollution > 0) {
        report->state = ST_ALERT;
        report->label = pollution[0];
        for (int i = 1; i < num_pollution; i++) {
            report->also[num_also] = pollution[i];
            num_also++;
        }
        for (int i = 0; i < num_watch; i++) {
            report->also[num_also] = watch[i];
            num_also++;
        }
    } else if (any_fault) {
        report->state = ST_FAULT;
        report->label = LBL_FAULT;
        for (int i = 0; i < num_watch; i++) {
            report->also[num_also] = watch[i];
            num_also++;
        }
    } else if (num_watch > 0) {
        report->state = ST_WATCH;
        report->label = watch[0];
        for (int i = 1; i < num_watch; i++) {
            report->also[num_also] = watch[i];
            num_also++;
        }
    } else {
        report->state = ST_NORMAL;
        if (rain) {
            report->label = LBL_RAIN;
        } else {
            report->label = LBL_NONE;
        }
    }
    if (rain && report->label != LBL_RAIN) {
        report->also[num_also] = LBL_RAIN;
        num_also++;
    }
    report->num_also = num_also;

    if (report->label == LBL_NONE || report->label == LBL_FAULT) {
        report->confidence = 0.0f;
    } else {
        report->confidence = engine->persistence.confidence[report->label];
    }
}

// Splits the labels that are on into pollution and watch lists, each one
// sorted by confidence (highest first)
void sort_active_labels(struct Node_engine *engine, enum Label_id pollution[], int *num_pollution,
    enum Label_id watch[], int *num_watch) {
    for (int i = LBL_NONE + 1; i < LBL_RAIN; i++) {
        enum Label_id id = (enum Label_id)i;
        if (engine->persistence.active[id]) {
            if (get_label_info(id).category == CAT_POLLUTION) {
                insert_by_confidence(&engine->persistence, pollution, num_pollution, id);
            } else {
                insert_by_confidence(&engine->persistence, watch, num_watch, id);
            }
        }
    }
}

// Puts id into list[0..n) so the list stays sorted by confidence, and adds 1 to n
void insert_by_confidence(struct Persistence *persistence, enum Label_id list[], int *n,
    enum Label_id id) {
    int j = *n;
    *n = *n + 1;
    while (j > 0 && persistence->confidence[list[j - 1]] < persistence->confidence[id]) {
        list[j] = list[j - 1];
        j--;
    }
    list[j] = id;
}
