// PC tests: baseline + persistence + the whole engine on the SIMULATE script.
// Run with: pio test -e native   (the native env builds with DEMO_MODE=1)
#include <unity.h>
#include <string.h>
#include "node_engine.h"
#include "sim.h"

#define MAX_TEST_EVENTS 8

struct Node_engine engine;
struct Baseline baseline;

void setUp(void) {
}

void tearDown(void) {
}

// Feeds one simulated minute into the engine and gives back its report.
// The events go into events[] and the number of them into num_events.
struct Node_report run_minute(unsigned long minute, struct Alert_event events[], int *num_events) {
    for (int k = 0; k < SIM_SAMPLES_PER_MIN; k++) {
        engine_add_sample(&engine, sim_reading(SIM_LOCAL, minute, k));
    }
    struct Node_report report;
    *num_events = engine_close_minute(&engine, minute * 60, &report, events, MAX_TEST_EVENTS);
    return report;
}

void test_median_ignores_spike(void) {
    float values[5] = { 7.1f, 7.2f, 14.0f, 7.15f, 7.18f };
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.18f, median_of(values, 5));
}

void test_baseline_warmup_and_freeze(void) {
    baseline_reset(&baseline);
    float values[S_COUNT] = { 7.0f, 200.0f, 8.0f, 17.0f };
    bool valid[S_COUNT] = { true, true, true, true };
    bool add[S_COUNT] = { true, true, true, true };
    for (int m = 0; m + 1 < BASELINE_MIN_MINUTES; m++) {
        baseline_add_minute(&baseline, values, valid, add);
    }
    TEST_ASSERT_FALSE(baseline_ready(&baseline));
    baseline_add_minute(&baseline, values, valid, add);
    TEST_ASSERT_TRUE(baseline_ready(&baseline));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, baseline.median[S_TDS]);

    // Frozen minutes must not move the baseline
    float polluted[S_COUNT] = { 4.0f, 900.0f, 40.0f, 17.0f };
    bool frozen[S_COUNT] = { false, false, false, false };
    for (int m = 0; m < 20; m++) {
        baseline_add_minute(&baseline, polluted, valid, frozen);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, baseline.median[S_TDS]);
}

void test_persistence_needs_three_minutes_on_five_off(void) {
    struct Persistence persistence;
    persistence_reset(&persistence);
    struct Rule_result hit;
    hit.id = LBL_SEWAGE;
    hit.confidence = 0.75f;
    struct Alert_event events[4];

    TEST_ASSERT_EQUAL(0, persistence_update(&persistence, &hit, 1, events, 4));
    TEST_ASSERT_EQUAL(0, persistence_update(&persistence, &hit, 1, events, 4));
    TEST_ASSERT_EQUAL(1, persistence_update(&persistence, &hit, 1, events, 4));
    TEST_ASSERT_TRUE(events[0].on);
    TEST_ASSERT_TRUE(persistence.active[LBL_SEWAGE]);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(0, persistence_update(&persistence, NULL, 0, events, 4));
    }
    TEST_ASSERT_EQUAL(1, persistence_update(&persistence, NULL, 0, events, 4));
    TEST_ASSERT_FALSE(events[0].on);
}

void test_engine_simulated_scenario(void) {
    engine_begin(&engine, "N1");
    bool saw_building = false;
    bool saw_rain = false;
    bool saw_alert_on = false;
    bool saw_alert_off = false;
    bool false_alarm_before_acid = false;
    struct Node_report report;

    for (unsigned long m = 0; m < SIM_CYCLE_MIN; m++) {
        struct Alert_event events[MAX_TEST_EVENTS];
        int num_events;
        report = run_minute(m, events, &num_events);
        if (report.state == ST_BASELINE_BUILDING) {
            saw_building = true;
        }
        if (report.label == LBL_RAIN) {
            saw_rain = true;
        }
        if (m < SIM_ACID_START && report.state == ST_ALERT) {
            false_alarm_before_acid = true;
        }
        for (int i = 0; i < num_events; i++) {
            if (events[i].id == LBL_HEAVY_METALS && events[i].on) {
                saw_alert_on = true;
            }
            if (events[i].id == LBL_HEAVY_METALS && !events[i].on) {
                saw_alert_off = true;
            }
        }
        if (m == SIM_ACID_START + PERSIST_ON_MIN + 1) {
            TEST_ASSERT_EQUAL(ST_ALERT, report.state);
            TEST_ASSERT_EQUAL(LBL_HEAVY_METALS, report.label);
            // The baseline stayed at normal levels during the spike
            TEST_ASSERT_FLOAT_WITHIN(0.2f, 7.2f, report.base[S_PH]);
        }
    }
    TEST_ASSERT_TRUE(saw_building);
    TEST_ASSERT_TRUE(saw_rain);
    TEST_ASSERT_FALSE(false_alarm_before_acid);
    TEST_ASSERT_TRUE(saw_alert_on);
    TEST_ASSERT_TRUE(saw_alert_off);
    TEST_ASSERT_EQUAL(ST_NORMAL, report.state);
}

// A pattern that keeps coming and going must not leak into "normal"
void test_baseline_held_after_pattern_clears(void) {
    engine_begin(&engine, "N1");
    struct Alert_event events[MAX_TEST_EVENTS];
    struct Node_report report;
    unsigned long m = 0;
    // Learn clean water
    for (; m < BASELINE_MIN_MINUTES + 5; m++) {
        for (int k = 0; k < SIM_SAMPLES_PER_MIN; k++) {
            struct Reading r = sim_reading(SIM_LOCAL, 0, k);
            engine_add_sample(&engine, r);
        }
        engine_close_minute(&engine, m * 60, &report, events, MAX_TEST_EVENTS);
    }
    float clean_tds = engine.baseline.median[S_TDS];
    // 3 minutes of salty water (a pattern), then 1 minute that looks clean-ish
    // but still has extra TDS, repeated: the extra TDS must not be learned
    for (int cycle = 0; cycle < 6; cycle++) {
        for (int i = 0; i < 4; i++, m++) {
            for (int k = 0; k < SIM_SAMPLES_PER_MIN; k++) {
                struct Reading r = sim_reading(SIM_LOCAL, 0, k);
                if (i < 3) {
                    r.value[S_TDS] = 1800.0f;      // salt rule fires
                } else {
                    r.value[S_TDS] *= 1.08f;       // no rule fires
                }
                engine_add_sample(&engine, r);
            }
            engine_close_minute(&engine, m * 60, &report, events, MAX_TEST_EVENTS);
        }
    }
    TEST_ASSERT_FLOAT_WITHIN(1.0f, clean_tds, engine.baseline.median[S_TDS]);
    TEST_ASSERT_TRUE(engine.hold_minutes_left > 0);
}

void test_engine_temp_sensor_missing_is_fault(void) {
    engine_begin(&engine, "N1");
    struct Node_report report;
    for (unsigned long m = 0; m < BASELINE_MIN_MINUTES + 2; m++) {
        for (int k = 0; k < SIM_SAMPLES_PER_MIN; k++) {
            struct Reading reading = sim_reading(SIM_LOCAL, m, k);
            reading.valid[S_TEMP] = false;
            engine_add_sample(&engine, reading);
        }
        struct Alert_event events[4];
        engine_close_minute(&engine, m * 60, &report, events, 4);
    }
    TEST_ASSERT_EQUAL(ST_FAULT, report.state);
    TEST_ASSERT_EQUAL(F_NO_READING, report.fault[S_TEMP]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_median_ignores_spike);
    RUN_TEST(test_baseline_warmup_and_freeze);
    RUN_TEST(test_persistence_needs_three_minutes_on_five_off);
    RUN_TEST(test_engine_simulated_scenario);
    RUN_TEST(test_baseline_held_after_pattern_clears);
    RUN_TEST(test_engine_temp_sensor_missing_is_fault);
    return UNITY_END();
}
