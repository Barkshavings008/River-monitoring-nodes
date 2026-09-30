// PC tests for the fault checks, rules and conversions: pio test -e native
#include <unity.h>
#include <stddef.h>
#include "rules.h"
#include "compensation.h"

// Makes rule inputs with the given readings and a normal baseline
struct Rule_inputs make_inputs(float ph, float tds, float ntu, float temp) {
    struct Rule_inputs in;
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
    in.has_tds_step_ref = false;
    in.tds_step_ref = 0.0f;
    in.has_day_range = false;
    in.ph_day_min = 0.0f;
    in.ph_day_max = 0.0f;
    return in;
}

// true if the label is in the output. Its confidence goes into *confidence
// (if confidence isn't NULL).
bool has_label(struct Rule_output out, enum Label_id id, float *confidence) {
    for (int i = 0; i < out.num_hits; i++) {
        if (out.hits[i].id == id) {
            if (confidence != NULL) {
                *confidence = out.hits[i].confidence;
            }
            return true;
        }
    }
    return false;
}

void setUp(void) {
}

void tearDown(void) {
}

void test_normal_water_fires_nothing(void) {
    struct Rule_output out = evaluate_rules(make_inputs(7.2f, 212.0f, 8.3f, 17.2f));
    TEST_ASSERT_EQUAL(0, out.num_hits);
    TEST_ASSERT_FALSE(out.rain);
}

void test_heavy_metals_full_confidence(void) {
    float confidence;
    struct Rule_output out = evaluate_rules(make_inputs(4.6f, 612.0f, 18.2f, 17.4f));
    TEST_ASSERT_TRUE(has_label(out, LBL_HEAVY_METALS, &confidence));
    // 3/3 + strong, capped at 1
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, confidence);
}

void test_heavy_metals_without_turbidity(void) {
    struct Rule_result result = rule_heavy_metals(make_inputs(4.6f, 612.0f, 8.1f, 17.1f));
    TEST_ASSERT_EQUAL(LBL_HEAVY_METALS, result.id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f / 3.0f + 0.1f, result.confidence);
}

void test_heavy_metals_needs_tds(void) {
    struct Rule_result result = rule_heavy_metals(make_inputs(4.6f, 230.0f, 18.2f, 17.1f));
    TEST_ASSERT_EQUAL(LBL_NONE, result.id);
}

void test_faulted_ph_suppresses_heavy_metals(void) {
    struct Rule_inputs in = make_inputs(4.6f, 612.0f, 18.2f, 17.4f);
    in.ok[S_PH] = false;
    TEST_ASSERT_FALSE(has_label(evaluate_rules(in), LBL_HEAVY_METALS, NULL));
}

void test_rain_filters_pollution_and_sediment(void) {
    struct Rule_output out = evaluate_rules(make_inputs(7.2f, 168.0f, 26.0f, 16.3f));
    TEST_ASSERT_TRUE(out.rain);
    TEST_ASSERT_TRUE(has_label(out, LBL_RAIN, NULL));
    TEST_ASSERT_FALSE(has_label(out, LBL_SEDIMENT, NULL));
    TEST_ASSERT_FALSE(has_label(out, LBL_SEWAGE, NULL));
}

void test_sediment_without_rain(void) {
    struct Rule_output out = evaluate_rules(make_inputs(7.2f, 212.0f, 30.0f, 17.1f));
    TEST_ASSERT_TRUE(has_label(out, LBL_SEDIMENT, NULL));
}

void test_industrial_step_change(void) {
    struct Rule_inputs in = make_inputs(5.5f, 400.0f, 8.1f, 19.5f);
    in.has_tds_step_ref = true;
    in.tds_step_ref = 210.0f;
    struct Rule_result result = rule_industrial(in);
    TEST_ASSERT_EQUAL(LBL_INDUSTRIAL, result.id);
    // pH + step + temp rise
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, result.confidence);
    // Slow rise, not a step
    in.tds_step_ref = 390.0f;
    TEST_ASSERT_EQUAL(LBL_NONE, rule_industrial(in).id);
}

void test_alkaline(void) {
    struct Rule_result result = rule_alkaline(make_inputs(10.3f, 260.0f, 8.1f, 17.1f));
    TEST_ASSERT_EQUAL(LBL_ALKALINE, result.id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f / 3.0f + 0.1f, result.confidence);
}

void test_sewage(void) {
    struct Rule_result result = rule_sewage(make_inputs(6.7f, 280.0f, 20.0f, 18.0f));
    TEST_ASSERT_EQUAL(LBL_SEWAGE, result.id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, result.confidence);
}

void test_thermal(void) {
    struct Rule_output out = evaluate_rules(make_inputs(7.2f, 212.0f, 8.1f, 20.0f));
    TEST_ASSERT_TRUE(has_label(out, LBL_THERMAL, NULL));
}

void test_salt(void) {
    struct Rule_output out = evaluate_rules(make_inputs(7.25f, 1800.0f, 8.5f, 17.1f));
    TEST_ASSERT_TRUE(has_label(out, LBL_SALT, NULL));
}

void test_nutrients_needs_day_range(void) {
    struct Rule_inputs in = make_inputs(8.0f, 245.0f, 8.1f, 26.0f);
    TEST_ASSERT_EQUAL(LBL_NONE, rule_nutrients(in).id);
    in.has_day_range = true;
    in.ph_day_min = 6.8f;
    in.ph_day_max = 8.4f;
    struct Rule_result result = rule_nutrients(in);
    TEST_ASSERT_EQUAL(LBL_NUTRIENTS, result.id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, result.confidence);
}

void test_fault_range_and_no_reading(void) {
    struct Fault_tracker tracker;
    fault_tracker_reset(&tracker);
    float values[S_COUNT] = { 15.0f, 200.0f, 5.0f, 17.0f };
    bool valid[S_COUNT] = { true, true, true, false };
    enum Fault_code faults[S_COUNT];
    check_faults(&tracker, values, valid, faults);
    TEST_ASSERT_EQUAL(F_OUT_OF_RANGE, faults[S_PH]);
    TEST_ASSERT_EQUAL(F_NONE, faults[S_TDS]);
    TEST_ASSERT_EQUAL(F_NO_READING, faults[S_TEMP]);
}

void test_fault_temp_jump(void) {
    struct Fault_tracker tracker;
    fault_tracker_reset(&tracker);
    float values[S_COUNT] = { 7.0f, 200.0f, 5.0f, 17.0f };
    bool valid[S_COUNT] = { true, true, true, true };
    enum Fault_code faults[S_COUNT];
    check_faults(&tracker, values, valid, faults);
    values[S_TEMP] = 21.0f;
    check_faults(&tracker, values, valid, faults);
    TEST_ASSERT_EQUAL(F_TEMP_JUMP, faults[S_TEMP]);
}

void test_fault_flatline_after_60_min(void) {
    struct Fault_tracker tracker;
    fault_tracker_reset(&tracker);
    float values[S_COUNT] = { 7.0f, 200.0f, 0.0f, 17.0f };
    bool valid[S_COUNT] = { true, true, true, true };
    enum Fault_code faults[S_COUNT];
    for (int m = 0; m < FLATLINE_MIN; m++) {
        check_faults(&tracker, values, valid, faults);
    }
    TEST_ASSERT_EQUAL(F_NONE, faults[S_PH]);
    check_faults(&tracker, values, valid, faults);
    TEST_ASSERT_EQUAL(F_FLATLINE, faults[S_PH]);
    // Clear water sitting at 0 NTU isn't "stuck"
    TEST_ASSERT_EQUAL(F_NONE, faults[S_NTU]);
}

void test_conversions(void) {
    TEST_ASSERT_FALSE(temperature_valid(-127.0f));
    TEST_ASSERT_FALSE(temperature_valid(85.0f));
    TEST_ASSERT_TRUE(temperature_valid(17.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, compensation_temp(-127.0f, false));

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 367.47f, tds_from_voltage(1.0f, 25.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 306.23f, tds_from_voltage(1.0f, 35.0f));

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.0f, ph_from_voltage(PH_V7));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, ph_from_voltage(PH_V4));

    TEST_ASSERT_TRUE(ntu_from_voltage(TURB_V_CLEAR) < 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, TURB_MAX_NTU, ntu_from_voltage(2.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ntu_from_voltage(5.0f));
}

int main(void) {
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
