// PC tests for the water speed estimate: pio test -e native
#include <unity.h>
#include <string.h>
#include "water_speed.h"

struct River_network network;
struct Speed_tracker tracker;
struct Speed_estimate found[4];

// A report with only the state and label filled in
struct Node_report make_report(enum Label_id label) {
    struct Node_report report;
    memset(&report, 0, sizeof(struct Node_report));
    report.label = label;
    if (label == LBL_NONE) {
        report.state = ST_NORMAL;
    } else {
        report.state = ST_ALERT;
    }
    return report;
}

// Gives every node a report for this minute (labels in node order: N0, N1, N2, N3)
// and returns how many new speeds were found
int run_minute(unsigned long minute, enum Label_id n0, enum Label_id n1, enum Label_id n2,
    enum Label_id n3) {
    network_update_report(&network, "N0", make_report(n0), minute);
    network_update_report(&network, "N1", make_report(n1), minute);
    network_update_report(&network, "N2", make_report(n2), minute);
    network_update_report(&network, "N3", make_report(n3), minute);
    return speed_tracker_update(&tracker, &network, minute, found, 4);
}

// Same 4 node river as the network tests: N0 at 0 m, N1 and N2 at 1000 m, N3 at 2500 m
void setUp(void) {
    network_clear(&network);
    network_add_node(&network, "N0", 0, "upstream", false);
    network_add_node(&network, "N1", 1000, "left bank", true);
    network_add_node(&network, "N2", 1000, "right bank", false);
    network_add_node(&network, "N3", 2500, "downstream", false);
    speed_tracker_begin(&tracker);
}

void tearDown(void) {
}

void test_plume_travelling_down_gives_speed(void) {
    for (unsigned long m = 1; m <= 5; m++) {
        TEST_ASSERT_EQUAL(0, run_minute(m, LBL_NONE, LBL_NONE, LBL_NONE, LBL_NONE));
    }
    // Arrives at N0 at minute 10, N1 at minute 30 (1000 m in 20 min)
    TEST_ASSERT_EQUAL(0, run_minute(10, LBL_SEWAGE, LBL_NONE, LBL_NONE, LBL_NONE));
    TEST_ASSERT_EQUAL(1, run_minute(30, LBL_SEWAGE, LBL_SEWAGE, LBL_NONE, LBL_NONE));
    TEST_ASSERT_EQUAL_STRING("N0", found[0].from_id);
    TEST_ASSERT_EQUAL_STRING("N1", found[0].to_id);
    TEST_ASSERT_EQUAL_UINT32(1000, found[0].metres);
    TEST_ASSERT_EQUAL_UINT32(20, found[0].minutes);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.833f, found[0].metres_per_sec);
    TEST_ASSERT_EQUAL(LBL_SEWAGE, found[0].label);
    TEST_ASSERT_TRUE(tracker.has_estimate);

    // Then N3 at minute 55: paired with the closest upstream arrival (N1, 1500 m in 25 min)
    TEST_ASSERT_EQUAL(1, run_minute(55, LBL_SEWAGE, LBL_SEWAGE, LBL_NONE, LBL_SEWAGE));
    TEST_ASSERT_EQUAL_STRING("N1", found[0].from_id);
    TEST_ASSERT_EQUAL_UINT32(1500, found[0].metres);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, tracker.latest.metres_per_sec);
}

void test_different_pollution_is_not_paired(void) {
    run_minute(1, LBL_NONE, LBL_NONE, LBL_NONE, LBL_NONE);
    run_minute(10, LBL_SEWAGE, LBL_NONE, LBL_NONE, LBL_NONE);
    TEST_ASSERT_EQUAL(0, run_minute(30, LBL_SEWAGE, LBL_HEAVY_METALS, LBL_NONE, LBL_NONE));
    TEST_ASSERT_FALSE(tracker.has_estimate);
}

void test_same_spot_and_upstream_order_not_paired(void) {
    run_minute(1, LBL_NONE, LBL_NONE, LBL_NONE, LBL_NONE);
    // N1 then N2 at the same distance: 0 m apart, no speed
    run_minute(10, LBL_NONE, LBL_SEWAGE, LBL_NONE, LBL_NONE);
    TEST_ASSERT_EQUAL(0, run_minute(15, LBL_NONE, LBL_SEWAGE, LBL_SEWAGE, LBL_NONE));
    // N0 after N1: pollution can't flow uphill
    TEST_ASSERT_EQUAL(0, run_minute(20, LBL_SEWAGE, LBL_SEWAGE, LBL_SEWAGE, LBL_NONE));
    TEST_ASSERT_FALSE(tracker.has_estimate);
}

void test_rain_in_between_blocks_pairing(void) {
    run_minute(1, LBL_NONE, LBL_NONE, LBL_NONE, LBL_NONE);
    run_minute(10, LBL_SEWAGE, LBL_NONE, LBL_NONE, LBL_NONE);
    run_minute(20, LBL_RAIN, LBL_RAIN, LBL_RAIN, LBL_RAIN);
    TEST_ASSERT_EQUAL(0, run_minute(30, LBL_SEWAGE, LBL_SEWAGE, LBL_NONE, LBL_NONE));
    TEST_ASSERT_FALSE(tracker.has_estimate);
}

void test_too_old_or_too_fast_ignored(void) {
    run_minute(1, LBL_NONE, LBL_NONE, LBL_NONE, LBL_NONE);
    run_minute(10, LBL_SEWAGE, LBL_NONE, LBL_NONE, LBL_NONE);
    // 1000 m in 1 min = 16.7 m/s: not a real river
    TEST_ASSERT_EQUAL(0, run_minute(11, LBL_SEWAGE, LBL_SEWAGE, LBL_NONE, LBL_NONE));
    // N3 more than 6 hours after the arrivals upstream
    TEST_ASSERT_EQUAL(0, run_minute(10 + SPEED_PAIR_WINDOW_MIN + 20, LBL_SEWAGE, LBL_SEWAGE,
        LBL_NONE, LBL_SEWAGE));
    TEST_ASSERT_FALSE(tracker.has_estimate);
}

void test_only_fresh_arrivals_count(void) {
    run_minute(1, LBL_NONE, LBL_NONE, LBL_NONE, LBL_NONE);
    run_minute(10, LBL_SEWAGE, LBL_NONE, LBL_NONE, LBL_NONE);
    // N1 already had a different alert a moment ago, so sewage turning on
    // there isn't a fresh arrival
    run_minute(20, LBL_SEWAGE, LBL_THERMAL, LBL_NONE, LBL_NONE);
    TEST_ASSERT_EQUAL(0, run_minute(30, LBL_SEWAGE, LBL_SEWAGE, LBL_NONE, LBL_NONE));
    TEST_ASSERT_FALSE(tracker.has_estimate);

    // Labels already on when a node is first seen don't count either: N0's
    // sewage could have started hours ago, so it can't be paired with N1's
    speed_tracker_begin(&tracker);
    TEST_ASSERT_EQUAL(0, run_minute(31, LBL_SEWAGE, LBL_NONE, LBL_NONE, LBL_NONE));
    TEST_ASSERT_EQUAL(0, run_minute(50, LBL_SEWAGE, LBL_SEWAGE, LBL_NONE, LBL_NONE));
    TEST_ASSERT_FALSE(tracker.has_estimate);
}

void test_each_report_only_looked_at_once(void) {
    run_minute(1, LBL_NONE, LBL_NONE, LBL_NONE, LBL_NONE);
    run_minute(10, LBL_SEWAGE, LBL_NONE, LBL_NONE, LBL_NONE);
    TEST_ASSERT_EQUAL(1, run_minute(30, LBL_SEWAGE, LBL_SEWAGE, LBL_NONE, LBL_NONE));
    // Nothing new arrived, so calling it again finds nothing
    TEST_ASSERT_EQUAL(0, speed_tracker_update(&tracker, &network, 30, found, 4));
    TEST_ASSERT_EQUAL(0, speed_tracker_update(&tracker, &network, 31, found, 4));
}

void test_across_a_stream_join(void) {
    // stream_a (2000 m long) joins main at 500 m; S1 near the top of the stream
    network_clear(&network);
    network_add_branch(&network, "main", 0, NULL, 0);
    network_add_branch(&network, "stream_a", 2000, "main", 500);
    network_add_node_on_branch(&network, "S1", "stream_a", 500, "stream", false);
    network_add_node_on_branch(&network, "M1", "main", 1100, "below join", true);
    speed_tracker_begin(&tracker);

    network_update_report(&network, "S1", make_report(LBL_NONE), 1);
    network_update_report(&network, "M1", make_report(LBL_NONE), 1);
    speed_tracker_update(&tracker, &network, 1, found, 4);
    network_update_report(&network, "S1", make_report(LBL_INDUSTRIAL), 5);
    network_update_report(&network, "M1", make_report(LBL_NONE), 5);
    speed_tracker_update(&tracker, &network, 5, found, 4);
    network_update_report(&network, "S1", make_report(LBL_INDUSTRIAL), 40);
    network_update_report(&network, "M1", make_report(LBL_INDUSTRIAL), 40);
    TEST_ASSERT_EQUAL(1, speed_tracker_update(&tracker, &network, 40, found, 4));
    // 1500 m left on the stream + 600 m down main = 2100 m in 35 min = 1.0 m/s
    TEST_ASSERT_EQUAL_UINT32(2100, found[0].metres);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, found[0].metres_per_sec);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_plume_travelling_down_gives_speed);
    RUN_TEST(test_different_pollution_is_not_paired);
    RUN_TEST(test_same_spot_and_upstream_order_not_paired);
    RUN_TEST(test_rain_in_between_blocks_pairing);
    RUN_TEST(test_too_old_or_too_fast_ignored);
    RUN_TEST(test_only_fresh_arrivals_count);
    RUN_TEST(test_each_report_only_looked_at_once);
    RUN_TEST(test_across_a_stream_join);
    return UNITY_END();
}
