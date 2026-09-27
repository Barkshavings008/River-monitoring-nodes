// PC unit tests for fault checks, rules and conversions: pio test -e native
#include <unity.h>
#include "rules.h"
#include "compensation.h"
#include "config.h"

// Makes rule inputs with the given readings and a fixed normal baseline.
static RuleInputs inputs(float ph, float tds, float ntu, float temp) {
    RuleInputs in;
    in.now[S_PH] = ph;
    in.base[S_PH] = 7.2f;
    in.now[S_TDS] = tds;
    in.base[S_TDS] = 210.0f;
    in.now[S_NTU] = ntu;
    in.base[S_NTU] = 8.1f;
    in.now[S_TEMP] = temp;
    in.base[S_TEMP] = 17.1f;
    for (int s = 0; s < S_COUNT; s++) {
        in.ok[s] = true;
    }
    in.hasTdsStepRef = false;
    in.tdsStepRef = 0.0f;
    in.hasDayRange = false;
    in.phDayMin = 0.0f;
    in.phDayMax = 0.0f;
    return in;
}

// True if label id is in the output. Its confidence goes into conf if
// conf is not NULL.
static bool hasLabel(const RuleOutput &o, LabelId id, float *conf = NULL) {
    for (int i = 0; i < o.n; i++) {
        if (o.hits[i].id == id) {
            if (conf != NULL) {
                *conf = o.hits[i].conf;
            }
            return true;
        }
    }
    return false;
}

void setUp() {
}

void tearDown() {
}

void test_normal_water_fires_nothing() {
    RuleOutput o = evaluateRules(inputs(7.2f, 212.0f, 8.3f, 17.2f));
    TEST_ASSERT_EQUAL(0, o.n);
    TEST_ASSERT_FALSE(o.rain);
}

void test_heavy_metals_full_confidence() {
    float conf;
    RuleOutput o = evaluateRules(inputs(4.6f, 612.0f, 18.2f, 17.4f));
    TEST_ASSERT_TRUE(hasLabel(o, LBL_HEAVY_METALS, &conf));
    // 3/3 + strong, capped.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, conf);
}

void test_heavy_metals_without_turbidity() {
    RuleResult r = ruleHeavyMetals(inputs(4.6f, 612.0f, 8.1f, 17.1f));
    TEST_ASSERT_EQUAL(LBL_HEAVY_METALS, r.id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f / 3.0f + 0.1f, r.conf);
}

void test_heavy_metals_needs_tds() {
    RuleResult r = ruleHeavyMetals(inputs(4.6f, 230.0f, 18.2f, 17.1f));
    TEST_ASSERT_EQUAL(LBL_NONE, r.id);
}

void test_faulted_ph_suppresses_heavy_metals() {
    RuleInputs in = inputs(4.6f, 612.0f, 18.2f, 17.4f);
    in.ok[S_PH] = false;
    TEST_ASSERT_FALSE(hasLabel(evaluateRules(in), LBL_HEAVY_METALS));
}

void test_rain_filters_pollution_and_sediment() {
    RuleOutput o = evaluateRules(inputs(7.2f, 168.0f, 26.0f, 16.3f));
    TEST_ASSERT_TRUE(o.rain);
    TEST_ASSERT_TRUE(hasLabel(o, LBL_RAIN));
    TEST_ASSERT_FALSE(hasLabel(o, LBL_SEDIMENT));
    TEST_ASSERT_FALSE(hasLabel(o, LBL_SEWAGE));
}

void test_sediment_without_rain() {
    RuleOutput o = evaluateRules(inputs(7.2f, 212.0f, 30.0f, 17.1f));
    TEST_ASSERT_TRUE(hasLabel(o, LBL_SEDIMENT));
}

void test_industrial_step_change() {
    RuleInputs in = inputs(5.5f, 400.0f, 8.1f, 19.5f);
    in.hasTdsStepRef = true;
    in.tdsStepRef = 210.0f;
    RuleResult r = ruleIndustrial(in);
    TEST_ASSERT_EQUAL(LBL_INDUSTRIAL, r.id);
    // pH + step + temp rise.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r.conf);
    // Slow rise, not a step.
    in.tdsStepRef = 390.0f;
    TEST_ASSERT_EQUAL(LBL_NONE, ruleIndustrial(in).id);
}

void test_alkaline() {
    RuleResult r = ruleAlkaline(inputs(10.3f, 260.0f, 8.1f, 17.1f));
    TEST_ASSERT_EQUAL(LBL_ALKALINE, r.id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f / 3.0f + 0.1f, r.conf);
}

void test_sewage() {
    RuleResult r = ruleSewage(inputs(6.7f, 280.0f, 20.0f, 18.0f));
    TEST_ASSERT_EQUAL(LBL_SEWAGE, r.id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r.conf);
}

void test_thermal() {
    RuleOutput o = evaluateRules(inputs(7.2f, 212.0f, 8.1f, 20.0f));
    TEST_ASSERT_TRUE(hasLabel(o, LBL_THERMAL));
}

void test_salt() {
    RuleOutput o = evaluateRules(inputs(7.25f, 1800.0f, 8.5f, 17.1f));
    TEST_ASSERT_TRUE(hasLabel(o, LBL_SALT));
}

void test_nutrients_needs_day_range() {
    RuleInputs in = inputs(8.0f, 245.0f, 8.1f, 26.0f);
    TEST_ASSERT_EQUAL(LBL_NONE, ruleNutrients(in).id);
    in.hasDayRange = true;
    in.phDayMin = 6.8f;
    in.phDayMax = 8.4f;
    RuleResult r = ruleNutrients(in);
    TEST_ASSERT_EQUAL(LBL_NUTRIENTS, r.id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r.conf);
}

void test_fault_range_and_no_reading() {
    FaultTracker ft;
    ft.reset();
    float v[S_COUNT] = { 15.0f, 200.0f, 5.0f, 17.0f };
    bool valid[S_COUNT] = { true, true, true, false };
    uint8_t out[S_COUNT];
    checkFaults(ft, v, valid, out);
    TEST_ASSERT_EQUAL(F_OUT_OF_RANGE, out[S_PH]);
    TEST_ASSERT_EQUAL(F_NONE, out[S_TDS]);
    TEST_ASSERT_EQUAL(F_NO_READING, out[S_TEMP]);
}

void test_fault_temp_jump() {
    FaultTracker ft;
    ft.reset();
    float v[S_COUNT] = { 7.0f, 200.0f, 5.0f, 17.0f };
    bool valid[S_COUNT] = { true, true, true, true };
    uint8_t out[S_COUNT];
    checkFaults(ft, v, valid, out);
    v[S_TEMP] = 21.0f;
    checkFaults(ft, v, valid, out);
    TEST_ASSERT_EQUAL(F_TEMP_JUMP, out[S_TEMP]);
}

void test_fault_flatline_after_60_min() {
    FaultTracker ft;
    ft.reset();
    float v[S_COUNT] = { 7.0f, 200.0f, 0.0f, 17.0f };
    bool valid[S_COUNT] = { true, true, true, true };
    uint8_t out[S_COUNT];
    for (int m = 0; m < FLATLINE_MIN; m++) {
        checkFaults(ft, v, valid, out);
    }
    TEST_ASSERT_EQUAL(F_NONE, out[S_PH]);
    checkFaults(ft, v, valid, out);
    TEST_ASSERT_EQUAL(F_FLATLINE, out[S_PH]);
    // Clear water at 0 NTU is not "stuck".
    TEST_ASSERT_EQUAL(F_NONE, out[S_NTU]);
}

void test_conversions() {
    TEST_ASSERT_FALSE(temperatureValid(-127.0f));
    TEST_ASSERT_FALSE(temperatureValid(85.0f));
    TEST_ASSERT_TRUE(temperatureValid(17.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, compensationTemp(-127.0f, false));

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 367.47f, tdsFromVoltage(1.0f, 25.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 306.23f, tdsFromVoltage(1.0f, 35.0f));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.0f, phFromVoltage(PH_V7));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, phFromVoltage(PH_V4));

    TEST_ASSERT_TRUE(ntuFromVoltage(TURB_V_CLEAR) < 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, TURB_MAX_NTU, ntuFromVoltage(2.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ntuFromVoltage(5.0f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_normal_water_fires_nothing);
    RUN_TEST(test_heavy_metals_full_confidence);
    RUN_TEST(test_heavy_metals_without_turbidity);
    RUN_TEST(test_heavy_metals_needs_tds);
    RUN_TEST(test_faulted_ph_suppresses_heavy_metals);
    RUN_TEST(test_rain_filters_pollution_and_sediment);
    RUN_TEST(test_sediment_without_rain);
    RUN_TEST(test_industrial_step_change);
    RUN_TEST(test_alkaline);
    RUN_TEST(test_sewage);
    RUN_TEST(test_thermal);
    RUN_TEST(test_salt);
    RUN_TEST(test_nutrients_needs_day_range);
    RUN_TEST(test_fault_range_and_no_reading);
    RUN_TEST(test_fault_temp_jump);
    RUN_TEST(test_fault_flatline_after_60_min);
    RUN_TEST(test_conversions);
    return UNITY_END();
}
