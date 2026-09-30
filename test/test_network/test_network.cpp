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
    TEST_ASSERT_EQUAL(2, network_nodes_at(&network, 1000, indexes, MAX_NODES));
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
    return UNITY_END();
}
