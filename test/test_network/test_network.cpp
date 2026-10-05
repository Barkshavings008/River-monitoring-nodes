// PC tests for the array of river nodes: pio test -e native
#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "network.h"

struct River_network network;

// Makes a report with only the state, label and rain flag filled in
struct Node_report make_report(enum Node_state state, enum Label_id label, bool rain) {
    struct Node_report report;
    memset(&report, 0, sizeof(struct Node_report));
    report.state = state;
    report.label = label;
    report.rain_pattern = rain;
    return report;
}

// Builds the 4 node test network before every test
void setUp(void) {
    network_clear(&network);
    // Added out of order on purpose: the array has to keep itself sorted
    network_add_node(&network, "N3", 2500, "downstream", false);
    network_add_node(&network, "N0", 0, "upstream", false);
    network_add_node(&network, "N1", 1000, "left bank", true);
    network_add_node(&network, "N2", 1000, "right bank", false);
}

void tearDown(void) {
}

void test_sorted_insert_and_positions(void) {
    TEST_ASSERT_EQUAL(4, network.num_nodes);
    TEST_ASSERT_EQUAL_STRING("N0", network.nodes[0].id);
    TEST_ASSERT_EQUAL_STRING("N1", network.nodes[1].id);
    TEST_ASSERT_EQUAL_STRING("N2", network.nodes[2].id);
    TEST_ASSERT_EQUAL_STRING("N3", network.nodes[3].id);
    TEST_ASSERT_EQUAL(3, network_position_count(&network));
    TEST_ASSERT_EQUAL_UINT32(1000, network_position_distance(&network, 1));
    int indexes[MAX_NODES];
    // Two nodes at the same position
    TEST_ASSERT_EQUAL(2, network_nodes_at(&network, 0, 1000, indexes, MAX_NODES));
}

void test_duplicate_and_capacity(void) {
    TEST_ASSERT_FALSE(network_add_node(&network, "N1", 5, "dup", false));
    char id[16];
    for (int i = network.num_nodes; i < MAX_NODES; i++) {
        snprintf(id, 16, "X%d", i);
        TEST_ASSERT_TRUE(network_add_node(&network, id, 3000 + i, "x", false));
    }
    TEST_ASSERT_FALSE(network_add_node(&network, "ZZ", 1, "full", false));
}

void test_remove_frees_position(void) {
    TEST_ASSERT_TRUE(network_remove_node(&network, "N0"));
    TEST_ASSERT_EQUAL(3, network.num_nodes);
    TEST_ASSERT_EQUAL(2, network_position_count(&network));
    TEST_ASSERT_EQUAL_STRING("N1", network.nodes[0].id);
    TEST_ASSERT_FALSE(network_remove_node(&network, "N0"));
}

void test_move_keeps_report_and_order(void) {
    network_update_report(&network, "N2", make_report(ST_WATCH, LBL_SEDIMENT, false), 5);
    TEST_ASSERT_TRUE(network_move_node(&network, "N2", 2500));
    // Goes after the node already at that distance
    TEST_ASSERT_EQUAL_STRING("N3", network.nodes[2].id);
    TEST_ASSERT_EQUAL_STRING("N2", network.nodes[3].id);
    TEST_ASSERT_EQUAL(LBL_SEDIMENT, network.nodes[3].report.label);
    // New position between N0 and N1
    TEST_ASSERT_TRUE(network_move_node(&network, "N2", 500));
    TEST_ASSERT_EQUAL_STRING("N2", network.nodes[1].id);
    TEST_ASSERT_EQUAL(4, network_position_count(&network));
}

void test_assess_source_on_local_side(void) {
    network_update_report(&network, "N0", make_report(ST_NORMAL, LBL_NONE, false), 10);
    network_update_report(&network, "N1", make_report(ST_ALERT, LBL_HEAVY_METALS, false), 10);
    network_update_report(&network, "N2", make_report(ST_NORMAL, LBL_NONE, false), 10);
    struct Network_assessment a = network_assess(&network, "N1", 10);
    TEST_ASSERT_EQUAL(NF_SOURCE_LOCAL_SIDE, a.finding);
    TEST_ASSERT_TRUE(a.has_upstream);
    TEST_ASSERT_EQUAL_STRING("N0", a.upstream_id);
    TEST_ASSERT_EQUAL(1, a.siblings);
}

void test_assess_cross_section(void) {
    network_update_report(&network, "N0", make_report(ST_NORMAL, LBL_NONE, false), 10);
    network_update_report(&network, "N1", make_report(ST_ALERT, LBL_HEAVY_METALS, false), 10);
    network_update_report(&network, "N2", make_report(ST_ALERT, LBL_HEAVY_METALS, false), 10);
    TEST_ASSERT_EQUAL(NF_SOURCE_CROSS_SECTION, network_assess(&network, "N1", 10).finding);
}

void test_assess_from_upstream(void) {
    network_update_report(&network, "N0", make_report(ST_ALERT, LBL_HEAVY_METALS, false), 10);
    network_update_report(&network, "N1", make_report(ST_ALERT, LBL_HEAVY_METALS, false), 10);
    TEST_ASSERT_EQUAL(NF_FROM_UPSTREAM, network_assess(&network, "N1", 10).finding);
}

void test_assess_between_when_no_sibling_data(void) {
    network_remove_node(&network, "N2");
    network_update_report(&network, "N0", make_report(ST_NORMAL, LBL_NONE, false), 10);
    network_update_report(&network, "N1", make_report(ST_ALERT, LBL_SEWAGE, false), 10);
    TEST_ASSERT_EQUAL(NF_SOURCE_BETWEEN, network_assess(&network, "N1", 10).finding);
}

void test_assess_stale_upstream_ignored(void) {
    // N0's report is out of date by minute 10
    network_update_report(&network, "N0", make_report(ST_NORMAL, LBL_NONE, false), 1);
    network_update_report(&network, "N1", make_report(ST_ALERT, LBL_HEAVY_METALS, false), 10);
    network_update_report(&network, "N2", make_report(ST_NORMAL, LBL_NONE, false), 10);
    struct Network_assessment a = network_assess(&network, "N1", 10);
    TEST_ASSERT_FALSE(a.has_upstream);
    TEST_ASSERT_EQUAL(NF_SOURCE_LOCAL_SIDE, a.finding);
}

void test_assess_rain_confirmed_and_unconfirmed(void) {
    network_update_report(&network, "N0", make_report(ST_NORMAL, LBL_RAIN, true), 10);
    network_update_report(&network, "N1", make_report(ST_NORMAL, LBL_RAIN, true), 10);
    network_update_report(&network, "N2", make_report(ST_NORMAL, LBL_RAIN, true), 10);
    network_update_report(&network, "N3", make_report(ST_NORMAL, LBL_NONE, false), 10);
    TEST_ASSERT_EQUAL(NF_RAIN_CONFIRMED, network_assess(&network, "N1", 10).finding);

    network_update_report(&network, "N0", make_report(ST_NORMAL, LBL_NONE, false), 10);
    network_update_report(&network, "N2", make_report(ST_NORMAL, LBL_NONE, false), 10);
    TEST_ASSERT_EQUAL(NF_RAIN_UNCONFIRMED, network_assess(&network, "N1", 10).finding);
}

/////////////////////////
//////// BRANCHES ///////
/////////////////////////

// Two streams joining into the main river:
//
//   stream_a (2000 m long)  A1 at 500, A2 at 1800 --.
//                                                     >-- joins main at 0 m -- M1 at 300, M2 at 2000
//   stream_b (1500 m long)  B1 at 1200 -------------'
//   stream_c (800 m long) joins main at 1000 m, C1 at 700
void build_branched(void) {
    network_clear(&network);
    TEST_ASSERT_EQUAL(0, network_add_branch(&network, "main", 0, NULL, 0));
    TEST_ASSERT_EQUAL(1, network_add_branch(&network, "stream_a", 2000, "main", 0));
    TEST_ASSERT_EQUAL(2, network_add_branch(&network, "stream_b", 1500, "main", 0));
    TEST_ASSERT_EQUAL(3, network_add_branch(&network, "stream_c", 800, "main", 1000));
    // Added in a jumbled order on purpose
    network_add_node_on_branch(&network, "M2", "main", 2000, "town", false);
    network_add_node_on_branch(&network, "B1", "stream_b", 1200, "b", false);
    network_add_node_on_branch(&network, "M1", "main", 300, "below join", true);
    network_add_node_on_branch(&network, "A2", "stream_a", 1800, "a low", false);
    network_add_node_on_branch(&network, "C1", "stream_c", 700, "c", false);
    network_add_node_on_branch(&network, "A1", "stream_a", 500, "a high", false);
}

// Gives every node a normal report at minute 10
void all_normal(void) {
    for (int i = 0; i < network.num_nodes; i++) {
        network_update_report(&network, network.nodes[i].id, make_report(ST_NORMAL, LBL_NONE, false), 10);
    }
}

void test_branch_setup_rules(void) {
    network_clear(&network);
    TEST_ASSERT_EQUAL(-1, network_add_branch(&network, "stream", 100, "main", 0)); // main isn't there yet
    TEST_ASSERT_EQUAL(0, network_add_branch(&network, "main", 0, NULL, 0));
    TEST_ASSERT_EQUAL(-1, network_add_branch(&network, "main", 0, NULL, 0));      // name already used
    TEST_ASSERT_FALSE(network_add_node_on_branch(&network, "X", "nowhere", 0, "x", false));
    TEST_ASSERT_TRUE(network_add_node(&network, "X", 0, "x", false));
    TEST_ASSERT_EQUAL(1, network.num_branches);                                 // reused "main"
}

void test_branch_order_is_streams_first(void) {
    build_branched();
    TEST_ASSERT_EQUAL_STRING("A1", network.nodes[0].id);
    TEST_ASSERT_EQUAL_STRING("A2", network.nodes[1].id);
    TEST_ASSERT_EQUAL_STRING("B1", network.nodes[2].id);
    TEST_ASSERT_EQUAL_STRING("C1", network.nodes[3].id);
    TEST_ASSERT_EQUAL_STRING("M1", network.nodes[4].id);
    TEST_ASSERT_EQUAL_STRING("M2", network.nodes[5].id);
    TEST_ASSERT_EQUAL(6, network_position_count(&network));
}

void test_river_distance_follows_the_water(void) {
    build_branched();
    int a1 = network_find_node(&network, "A1");
    int a2 = network_find_node(&network, "A2");
    int b1 = network_find_node(&network, "B1");
    int m1 = network_find_node(&network, "M1");
    int m2 = network_find_node(&network, "M2");
    TEST_ASSERT_EQUAL(1300, network_river_distance(&network, a1, a2));
    TEST_ASSERT_EQUAL(1500 + 300, network_river_distance(&network, a1, m1));
    TEST_ASSERT_EQUAL(300 + 300, network_river_distance(&network, b1, m1));
    TEST_ASSERT_EQUAL(1700, network_river_distance(&network, m1, m2));
    TEST_ASSERT_EQUAL(-1, network_river_distance(&network, m1, a1));  // upstream, not down
    TEST_ASSERT_EQUAL(-1, network_river_distance(&network, a1, b1));  // different stream
}

void test_below_the_join_sees_both_streams(void) {
    build_branched();
    all_normal();
    network_update_report(&network, "M1", make_report(ST_ALERT, LBL_SEWAGE, false), 10);
    struct Network_assessment a = network_assess(&network, "M1", 10);
    TEST_ASSERT_EQUAL(NF_SOURCE_BETWEEN, a.finding);
    TEST_ASSERT_TRUE(a.branched);
    TEST_ASSERT_EQUAL(2, a.num_upstream);              // A2 (not A1) and B1
    TEST_ASSERT_EQUAL_STRING("A2, B1", a.upstream_list);
}

void test_pollution_from_one_stream(void) {
    build_branched();
    all_normal();
    network_update_report(&network, "B1", make_report(ST_ALERT, LBL_HEAVY_METALS, false), 10);
    network_update_report(&network, "M1", make_report(ST_ALERT, LBL_HEAVY_METALS, false), 10);
    struct Network_assessment a = network_assess(&network, "M1", 10);
    TEST_ASSERT_EQUAL(NF_FROM_UPSTREAM, a.finding);
    TEST_ASSERT_EQUAL_STRING("B1", a.upstream_id);     // the stream it came from
    TEST_ASSERT_EQUAL_STRING("stream_b", a.upstream_branch);
    TEST_ASSERT_EQUAL(600, a.upstream_gap_m);
}

void test_stream_joining_between_nodes(void) {
    build_branched();
    all_normal();
    // M2 is below where stream_c joins, so its upstream nodes are M1 and C1
    network_update_report(&network, "C1", make_report(ST_ALERT, LBL_SALT, false), 10);
    network_update_report(&network, "M2", make_report(ST_ALERT, LBL_SALT, false), 10);
    struct Network_assessment a = network_assess(&network, "M2", 10);
    TEST_ASSERT_EQUAL(NF_FROM_UPSTREAM, a.finding);
    TEST_ASSERT_EQUAL_STRING("C1", a.upstream_id);
    TEST_ASSERT_EQUAL(2, a.num_upstream);
}

void test_stale_stream_node_looks_further_up(void) {
    build_branched();
    all_normal();
    network_update_report(&network, "A2", make_report(ST_NORMAL, LBL_NONE, false), 1); // out of date
    network_update_report(&network, "M1", make_report(ST_ALERT, LBL_SEWAGE, false), 10);
    struct Network_assessment a = network_assess(&network, "M1", 10);
    TEST_ASSERT_EQUAL_STRING("A1, B1", a.upstream_list);
}

void test_top_node_has_nothing_upstream(void) {
    build_branched();
    all_normal();
    network_update_report(&network, "A1", make_report(ST_ALERT, LBL_HEAVY_METALS, false), 10);
    struct Network_assessment a = network_assess(&network, "A1", 10);
    TEST_ASSERT_EQUAL(NF_NO_UPSTREAM, a.finding);
    TEST_ASSERT_FALSE(a.has_upstream);
}

void test_too_long_names_are_refused(void) {
    network_clear(&network);
    TEST_ASSERT_EQUAL(-1, network_add_branch(&network, "a_very_long_name", 0, NULL, 0));
    TEST_ASSERT_EQUAL(0, network_add_branch(&network, "main", 0, NULL, 0));
    // 7 characters is fine, 8 would have been cut short and clashed
    TEST_ASSERT_TRUE(network_add_node(&network, "RIVERN1", 0, "x", false));
    TEST_ASSERT_FALSE(network_add_node(&network, "RIVERNODE1", 10, "x", false));
    TEST_ASSERT_FALSE(network_add_node(&network, "RIVERNODE2", 20, "x", false));
    TEST_ASSERT_EQUAL(1, network.num_nodes);
}

void test_move_node_to_other_branch(void) {
    build_branched();
    TEST_ASSERT_TRUE(network_move_node_to_branch(&network, "M2", "stream_b", 100));
    TEST_ASSERT_EQUAL_STRING("M2", network.nodes[2].id);   // now the first node on stream_b
    TEST_ASSERT_FALSE(network_move_node_to_branch(&network, "M2", "nowhere", 100));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sorted_insert_and_positions);
    RUN_TEST(test_duplicate_and_capacity);
    RUN_TEST(test_remove_frees_position);
    RUN_TEST(test_move_keeps_report_and_order);
    RUN_TEST(test_assess_source_on_local_side);
    RUN_TEST(test_assess_cross_section);
    RUN_TEST(test_assess_from_upstream);
    RUN_TEST(test_assess_between_when_no_sibling_data);
    RUN_TEST(test_assess_stale_upstream_ignored);
    RUN_TEST(test_assess_rain_confirmed_and_unconfirmed);
    RUN_TEST(test_branch_setup_rules);
    RUN_TEST(test_branch_order_is_streams_first);
    RUN_TEST(test_river_distance_follows_the_water);
    RUN_TEST(test_below_the_join_sees_both_streams);
    RUN_TEST(test_pollution_from_one_stream);
    RUN_TEST(test_stream_joining_between_nodes);
    RUN_TEST(test_stale_stream_node_looks_further_up);
    RUN_TEST(test_top_node_has_nothing_upstream);
    RUN_TEST(test_too_long_names_are_refused);
    RUN_TEST(test_move_node_to_other_branch);
    return UNITY_END();
}
