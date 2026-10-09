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
    struct Rule_output out = evaluate_rules(make_inputs(4.6f, 612.0f, 120.0f, 17.4f));
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

void test_rain_filters_sediment(void) {
    struct Rule_output out = evaluate_rules(make_inputs(7.2f, 168.0f, 120.0f, 16.3f));
    TEST_ASSERT_TRUE(out.rain);
    TEST_ASSERT_TRUE(has_label(out, LBL_RAIN, NULL));
    TEST_ASSERT_FALSE(has_label(out, LBL_SEDIMENT, NULL));
    // A storm that only adds 20 NTU is within the sensor's wobble
    TEST_ASSERT_FALSE(evaluate_rules(make_inputs(7.2f, 168.0f, 28.0f, 16.3f)).rain);
}

void test_sediment_without_rain(void) {
    struct Rule_output out = evaluate_rules(make_inputs(7.2f, 212.0f, 120.0f, 17.1f));
    TEST_ASSERT_TRUE(has_label(out, LBL_SEDIMENT, NULL));
}

// Normal is 8.1 NTU, but the sensor wobbles by tens of NTU near clear water,
// so 3x normal isn't enough: it has to rise by more than NTU_MIN_RISE too
void test_turbidity_wobble_is_not_a_rise(void) {
    // 3.7x normal, but only 22 NTU more
    struct Rule_output out = evaluate_rules(make_inputs(7.2f, 212.0f, 30.0f, 17.1f));
    TEST_ASSERT_FALSE(has_label(out, LBL_SEDIMENT, NULL));
    TEST_ASSERT_EQUAL(LBL_NONE, rule_sewage(make_inputs(6.7f, 280.0f, 30.0f, 18.0f)).id);
    // and wobble doesn't stop the "clear water" rules either
    TEST_ASSERT_EQUAL(LBL_EFFLUENT, rule_effluent(make_inputs(6.85f, 260.0f, 30.0f, 17.2f)).id);
    // A diluted sewage overflow (+35 NTU) is still a rise
    TEST_ASSERT_EQUAL(LBL_SEWAGE, rule_sewage(make_inputs(6.7f, 280.0f, 43.0f, 18.0f)).id);
}

// In a river that's already a bit murky (normal 20 NTU), doubling isn't
// enough on its own either: it has to be NTU_MIN_RISE more as well
void test_turbidity_rise_needed_in_murky_river(void) {
    struct Rule_inputs in = make_inputs(6.7f, 280.0f, 41.0f, 18.0f);
    in.base[S_NTU] = 20.0f;
    TEST_ASSERT_EQUAL(LBL_NONE, rule_sewage(in).id);       // 2.05x but only +21 NTU
    in.now[S_NTU] = 60.0f;
    TEST_ASSERT_EQUAL(LBL_SEWAGE, rule_sewage(in).id);     // 3x and +40 NTU

    struct Rule_inputs mud = make_inputs(7.2f, 212.0f, 52.0f, 17.1f);
    mud.base[S_NTU] = 30.0f;
    TEST_ASSERT_EQUAL(LBL_NONE, rule_sediment(mud).id);    // over 50 NTU but only +22
    mud.now[S_NTU] = 95.0f;
    TEST_ASSERT_EQUAL(LBL_SEDIMENT, rule_sediment(mud).id);
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
    struct Rule_result result = rule_sewage(make_inputs(6.7f, 280.0f, 100.0f, 18.0f));
    TEST_ASSERT_EQUAL(LBL_SEWAGE, result.id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, result.confidence);
    // A big spill that doubles TDS still counts (there's no upper limit)
    TEST_ASSERT_EQUAL(LBL_SEWAGE, rule_sewage(make_inputs(6.7f, 450.0f, 100.0f, 18.0f)).id);
}

void test_thermal(void) {
    // 2.9 C warmer is a sunny afternoon, 3.4 C is more than that
    TEST_ASSERT_FALSE(has_label(evaluate_rules(make_inputs(7.2f, 212.0f, 8.1f, 20.0f)), LBL_THERMAL, NULL));
    TEST_ASSERT_TRUE(has_label(evaluate_rules(make_inputs(7.2f, 212.0f, 8.1f, 20.5f)), LBL_THERMAL, NULL));
    // Over 30 C counts even if it's normal for the spot
    struct Rule_inputs in = make_inputs(7.2f, 212.0f, 8.1f, 29.5f);
    in.base[S_TEMP] = 28.5f;
    TEST_ASSERT_EQUAL(LBL_NONE, rule_thermal(in).id);
    in.now[S_TEMP] = 30.2f;
    TEST_ASSERT_EQUAL(LBL_THERMAL, rule_thermal(in).id);
}

void test_salt(void) {
    // The TDS sensor tops out around 1120 mg/L, so 1000 has to be enough
    struct Rule_output out = evaluate_rules(make_inputs(7.25f, 1000.0f, 8.5f, 17.1f));
    TEST_ASSERT_TRUE(has_label(out, LBL_SALT, NULL));
    // Seawater raises pH a bit: still salt
    TEST_ASSERT_EQUAL(LBL_SALT, rule_salt(make_inputs(7.9f, 1000.0f, 8.5f, 17.1f)).id);
    // but not by more than 1, or drop it by more than 0.3
    TEST_ASSERT_EQUAL(LBL_NONE, rule_salt(make_inputs(8.4f, 1000.0f, 8.5f, 17.1f)).id);
    TEST_ASSERT_EQUAL(LBL_NONE, rule_salt(make_inputs(6.8f, 1000.0f, 8.5f, 17.1f)).id);
    TEST_ASSERT_EQUAL(LBL_NONE, rule_salt(make_inputs(7.25f, 850.0f, 8.5f, 17.1f)).id);
}

void test_effluent_clear_water_ph_drop_tds_rise(void) {
    // Normal is pH 7.2, TDS 210, 8.1 NTU, 17.1 C
    struct Rule_output out = evaluate_rules(make_inputs(6.85f, 260.0f, 8.5f, 17.2f));
    TEST_ASSERT_TRUE(has_label(out, LBL_EFFLUENT, NULL));
    TEST_ASSERT_FALSE(has_label(out, LBL_SEWAGE, NULL));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, rule_effluent(make_inputs(6.85f, 260.0f, 8.5f, 17.2f)).confidence);
    // A slight warm-up adds confidence
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, rule_effluent(make_inputs(6.85f, 260.0f, 8.5f, 18.0f)).confidence);
}

void test_effluent_not_when_muddy_or_ph_steady(void) {
    // Muddy: that's the sewage rule instead
    struct Rule_output muddy = evaluate_rules(make_inputs(6.85f, 280.0f, 100.0f, 17.2f));
    TEST_ASSERT_FALSE(has_label(muddy, LBL_EFFLUENT, NULL));
    TEST_ASSERT_TRUE(has_label(muddy, LBL_SEWAGE, NULL));
    // pH didn't drop
    TEST_ASSERT_EQUAL(LBL_NONE, rule_effluent(make_inputs(7.15f, 260.0f, 8.5f, 17.2f)).id);
    // Only a small TDS rise (15 %, within cheap probe drift)
    TEST_ASSERT_EQUAL(LBL_NONE, rule_effluent(make_inputs(6.85f, 241.0f, 8.5f, 17.2f)).id);
    // Only a small pH drop (0.25)
    TEST_ASSERT_EQUAL(LBL_NONE, rule_effluent(make_inputs(6.95f, 260.0f, 8.5f, 17.2f)).id);
    // Big pH drop is acid, not this
    TEST_ASSERT_EQUAL(LBL_NONE, rule_effluent(make_inputs(5.8f, 260.0f, 8.5f, 17.2f)).id);
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
    // No TDS rise (fertiliser nitrate is only a few mg/L): still nutrients, less sure
    in.now[S_TDS] = 212.0f;
    result = rule_nutrients(in);
    TEST_ASSERT_EQUAL(LBL_NUTRIENTS, result.id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f / 3.0f, result.confidence);
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
    // Jumps 4 C: could be real, so not a fault yet
    values[S_TEMP] = 21.0f;
    check_faults(&tracker, values, valid, faults);
    TEST_ASSERT_EQUAL(F_NONE, faults[S_TEMP]);
    // Goes straight back: the probe glitched
    values[S_TEMP] = 17.2f;
    check_faults(&tracker, values, valid, faults);
    TEST_ASSERT_EQUAL(F_TEMP_JUMP, faults[S_TEMP]);

    // A jump that stays (a warm outflow reaching the probe) is never a fault
    values[S_TEMP] = 22.0f;
    check_faults(&tracker, values, valid, faults);
    values[S_TEMP] = 22.3f;
    check_faults(&tracker, values, valid, faults);
    TEST_ASSERT_EQUAL(F_NONE, faults[S_TEMP]);
    values[S_TEMP] = 22.1f;
    check_faults(&tracker, values, valid, faults);
    TEST_ASSERT_EQUAL(F_NONE, faults[S_TEMP]);
}

// Very salty water pins the TDS sensor at its top: that's salt, not a stuck
// sensor. A reading the board can't physically give is a wiring fault.
void test_fault_tds_at_its_top(void) {
    struct Fault_tracker tracker;
    fault_tracker_reset(&tracker);
    float values[S_COUNT] = { 7.5f, 0.0f, 5.0f, 12.0f };
    values[S_TDS] = tds_from_voltage(2.3f, 12.0f);   // cold water: about 1370 mg/L
    bool valid[S_COUNT] = { true, true, true, true };
    enum Fault_code faults[S_COUNT];
    for (int m = 0; m < FLATLINE_MIN + 10; m++) {
        values[S_PH] = 7.5f + 0.01f * (m % 3);       // keep pH moving
        check_faults(&tracker, values, valid, faults);
    }
    TEST_ASSERT_EQUAL(F_NONE, faults[S_TDS]);
    values[S_TDS] = tds_from_voltage(2.9f, 12.0f);
    check_faults(&tracker, values, valid, faults);
    TEST_ASSERT_EQUAL(F_OUT_OF_RANGE, faults[S_TDS]);
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
    RUN_TEST(test_rain_filters_sediment);
    RUN_TEST(test_sediment_without_rain);
    RUN_TEST(test_turbidity_wobble_is_not_a_rise);
    RUN_TEST(test_turbidity_rise_needed_in_murky_river);
    RUN_TEST(test_industrial_step_change);
    RUN_TEST(test_alkaline);
    RUN_TEST(test_sewage);
    RUN_TEST(test_thermal);
    RUN_TEST(test_salt);
    RUN_TEST(test_effluent_clear_water_ph_drop_tds_rise);
    RUN_TEST(test_effluent_not_when_muddy_or_ph_steady);
    RUN_TEST(test_nutrients_needs_day_range);
    RUN_TEST(test_fault_range_and_no_reading);
    RUN_TEST(test_fault_temp_jump);
    RUN_TEST(test_fault_tds_at_its_top);
    RUN_TEST(test_fault_flatline_after_60_min);
    RUN_TEST(test_conversions);
    return UNITY_END();
}
