// PC tests: baseline + persistence + full engine on the SIMULATE script.
// Run with: pio test -e native   (native env builds with DEMO_MODE=1)
#include <unity.h>
#include <string.h>
#include "node_engine.h"
#include "sim.h"

#define MAX_TEST_EVENTS 8

static NodeEngine engine;
static Baseline baseline;

void setUp() {
}

void tearDown() {
}

// Feeds one simulated minute into the engine and returns its report.
// If ev is not NULL, the alert events are written to ev and their count
// to nEv.
static NodeReport runMinute(uint32_t minute, AlertEvent *ev = NULL,
                            uint8_t *nEv = NULL) {
    for (uint8_t k = 0; k < SIM_SAMPLES_PER_MIN; k++) {
        engine.addSample(simReading(SIM_LOCAL, minute, k));
    }
    NodeReport rep;
    AlertEvent localEv[MAX_TEST_EVENTS];
    uint8_t n;
    if (ev == NULL) {
        ev = localEv;
    }
    engine.closeMinute(minute * 60, rep, ev, MAX_TEST_EVENTS, n);
    if (nEv != NULL) {
        *nEv = n;
    }
    return rep;
}

void test_median_ignores_spike() {
    float v[5] = { 7.1f, 7.2f, 14.0f, 7.15f, 7.18f };
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.18f, medianOf(v, 5));
}

void test_baseline_warmup_and_freeze() {
    baseline.reset();
    float v[S_COUNT] = { 7.0f, 200.0f, 8.0f, 17.0f };
    bool valid[S_COUNT] = { true, true, true, true };
    bool add[S_COUNT] = { true, true, true, true };
    for (uint16_t m = 0; m + 1 < BASELINE_MIN_MINUTES; m++) {
        baseline.addMinute(v, valid, add);
    }
    TEST_ASSERT_FALSE(baseline.ready());
    baseline.addMinute(v, valid, add);
    TEST_ASSERT_TRUE(baseline.ready());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, baseline.value(S_TDS));

    // Frozen minutes must not move the baseline.
    float polluted[S_COUNT] = { 4.0f, 900.0f, 40.0f, 17.0f };
    bool frozen[S_COUNT] = { false, false, false, false };
    for (int m = 0; m < 20; m++) {
        baseline.addMinute(polluted, valid, frozen);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, baseline.value(S_TDS));
}

void test_persistence_needs_three_minutes_on_five_off() {
    Persistence p;
    p.reset();
    RuleResult hit = { LBL_SEWAGE, 0.75f };
    AlertEvent ev[4];
    TEST_ASSERT_EQUAL(0, p.update(&hit, 1, ev, 4));
    TEST_ASSERT_EQUAL(0, p.update(&hit, 1, ev, 4));
    TEST_ASSERT_EQUAL(1, p.update(&hit, 1, ev, 4));
    TEST_ASSERT_TRUE(ev[0].on);
    TEST_ASSERT_TRUE(p.active(LBL_SEWAGE));
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(0, p.update(NULL, 0, ev, 4));
    }
    TEST_ASSERT_EQUAL(1, p.update(NULL, 0, ev, 4));
    TEST_ASSERT_FALSE(ev[0].on);
}

void test_engine_simulated_scenario() {
    engine.begin("N1");
    bool sawBuilding = false;
    bool sawRain = false;
    bool sawAlertOn = false;
    bool sawAlertOff = false;
    bool falseAlarmBeforeAcid = false;
    NodeReport rep;

    for (uint32_t m = 0; m < SIM_CYCLE_MIN; m++) {
        AlertEvent ev[MAX_TEST_EVENTS];
        uint8_t n;
        rep = runMinute(m, ev, &n);
        if (rep.state == ST_BASELINE_BUILDING) {
            sawBuilding = true;
        }
        if (rep.label == LBL_RAIN) {
            sawRain = true;
        }
        if (m < SIM_ACID_START && rep.state == ST_ALERT) {
            falseAlarmBeforeAcid = true;
        }
        for (uint8_t i = 0; i < n; i++) {
            if (ev[i].id == LBL_HEAVY_METALS && ev[i].on) {
                sawAlertOn = true;
            }
            if (ev[i].id == LBL_HEAVY_METALS && !ev[i].on) {
                sawAlertOff = true;
            }
        }
        if (m == SIM_ACID_START + PERSIST_ON_MIN + 1) {
            TEST_ASSERT_EQUAL(ST_ALERT, rep.state);
            TEST_ASSERT_EQUAL(LBL_HEAVY_METALS, rep.label);
            // Baseline stayed at normal levels during the spike.
            TEST_ASSERT_FLOAT_WITHIN(0.2f, 7.2f, rep.base[S_PH]);
        }
    }
    TEST_ASSERT_TRUE(sawBuilding);
    TEST_ASSERT_TRUE(sawRain);
    TEST_ASSERT_FALSE(falseAlarmBeforeAcid);
    TEST_ASSERT_TRUE(sawAlertOn);
    TEST_ASSERT_TRUE(sawAlertOff);
    TEST_ASSERT_EQUAL(ST_NORMAL, rep.state);
}

void test_engine_temp_sensor_missing_is_fault() {
    engine.begin("N1");
    NodeReport rep;
    for (uint32_t m = 0; m < BASELINE_MIN_MINUTES + 2; m++) {
        for (uint8_t k = 0; k < SIM_SAMPLES_PER_MIN; k++) {
            Reading r = simReading(SIM_LOCAL, m, k);
            r.valid[S_TEMP] = false;
            engine.addSample(r);
        }
        AlertEvent ev[4];
        uint8_t n;
        engine.closeMinute(m * 60, rep, ev, 4, n);
    }
    TEST_ASSERT_EQUAL(ST_FAULT, rep.state);
    TEST_ASSERT_EQUAL(F_NO_READING, rep.fault[S_TEMP]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_median_ignores_spike);
    RUN_TEST(test_baseline_warmup_and_freeze);
    RUN_TEST(test_persistence_needs_three_minutes_on_five_off);
    RUN_TEST(test_engine_simulated_scenario);
    RUN_TEST(test_engine_temp_sensor_missing_is_fault);
    return UNITY_END();
}
