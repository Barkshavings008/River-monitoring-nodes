// PC unit tests for the array-based node network: pio test -e native
#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "network.h"

static RiverNetwork net;

// Builds the 4-node test network.
static void buildDefault() {
    net.clear();
    // Deliberately out of order: the array must keep itself sorted.
    net.addNode("N3", 2500, "downstream");
    net.addNode("N0", 0, "upstream");
    net.addNode("N1", 1000, "left bank", true);
    net.addNode("N2", 1000, "right bank");
}

// Makes a report with just a state, a label and the rain flag set.
static NodeReport report(NodeState st, LabelId label, bool rain = false) {
    NodeReport r;
    memset(&r, 0, sizeof(r));
    r.state = st;
    r.label = label;
    r.rainPattern = rain;
    return r;
}

void setUp() {
    buildDefault();
}

void tearDown() {
}

void test_sorted_insert_and_positions() {
    TEST_ASSERT_EQUAL(4, net.count());
    TEST_ASSERT_EQUAL_STRING("N0", net.at(0).id);
    TEST_ASSERT_EQUAL_STRING("N1", net.at(1).id);
    TEST_ASSERT_EQUAL_STRING("N2", net.at(2).id);
    TEST_ASSERT_EQUAL_STRING("N3", net.at(3).id);
    TEST_ASSERT_EQUAL(3, net.positionCount());
    TEST_ASSERT_EQUAL_UINT32(1000, net.positionDistance(1));
    uint8_t idx[MAX_NODES];
    // Two nodes at the same position.
    TEST_ASSERT_EQUAL(2, net.nodesAt(1000, idx, MAX_NODES));
}

void test_duplicate_and_capacity() {
    TEST_ASSERT_FALSE(net.addNode("N1", 5, "dup"));
    char id[8];
    for (int i = net.count(); i < MAX_NODES; i++) {
        snprintf(id, sizeof(id), "X%d", i);
        TEST_ASSERT_TRUE(net.addNode(id, 3000 + i, "x"));
    }
    TEST_ASSERT_FALSE(net.addNode("ZZ", 1, "full"));
}

void test_remove_frees_position() {
    TEST_ASSERT_TRUE(net.removeNode("N0"));
    TEST_ASSERT_EQUAL(3, net.count());
    TEST_ASSERT_EQUAL(2, net.positionCount());
    TEST_ASSERT_EQUAL_STRING("N1", net.at(0).id);
    TEST_ASSERT_FALSE(net.removeNode("N0"));
}

void test_divert_keeps_report_and_order() {
    net.updateReport("N2", report(ST_WATCH, LBL_SEDIMENT), 5);
    TEST_ASSERT_TRUE(net.divertNode("N2", 2500));
    // Joins after the existing sibling.
    TEST_ASSERT_EQUAL_STRING("N3", net.at(2).id);
    TEST_ASSERT_EQUAL_STRING("N2", net.at(3).id);
    TEST_ASSERT_EQUAL(LBL_SEDIMENT, net.at(3).report.label);
    // New position between N0 and N1.
    TEST_ASSERT_TRUE(net.divertNode("N2", 500));
    TEST_ASSERT_EQUAL_STRING("N2", net.at(1).id);
    TEST_ASSERT_EQUAL(4, net.positionCount());
}

void test_assess_source_on_local_side() {
    net.updateReport("N0", report(ST_NORMAL, LBL_NONE), 10);
    net.updateReport("N1", report(ST_ALERT, LBL_HEAVY_METALS), 10);
    net.updateReport("N2", report(ST_NORMAL, LBL_NONE), 10);
    NetworkAssessment a = net.assess("N1", 10);
    TEST_ASSERT_EQUAL(NF_SOURCE_LOCAL_SIDE, a.finding);
    TEST_ASSERT_TRUE(a.hasUpstream);
    TEST_ASSERT_EQUAL_STRING("N0", a.upstreamId);
    TEST_ASSERT_EQUAL(1, a.siblings);
}

void test_assess_cross_section() {
    net.updateReport("N0", report(ST_NORMAL, LBL_NONE), 10);
    net.updateReport("N1", report(ST_ALERT, LBL_HEAVY_METALS), 10);
    net.updateReport("N2", report(ST_ALERT, LBL_HEAVY_METALS), 10);
    TEST_ASSERT_EQUAL(NF_SOURCE_CROSS_SECTION, net.assess("N1", 10).finding);
}

void test_assess_from_upstream() {
    net.updateReport("N0", report(ST_ALERT, LBL_HEAVY_METALS), 10);
    net.updateReport("N1", report(ST_ALERT, LBL_HEAVY_METALS), 10);
    TEST_ASSERT_EQUAL(NF_FROM_UPSTREAM, net.assess("N1", 10).finding);
}

void test_assess_between_when_no_sibling_data() {
    net.removeNode("N2");
    net.updateReport("N0", report(ST_NORMAL, LBL_NONE), 10);
    net.updateReport("N1", report(ST_ALERT, LBL_SEWAGE), 10);
    TEST_ASSERT_EQUAL(NF_SOURCE_BETWEEN, net.assess("N1", 10).finding);
}

void test_assess_stale_upstream_ignored() {
    // N0's report is stale by minute 10.
    net.updateReport("N0", report(ST_NORMAL, LBL_NONE), 1);
    net.updateReport("N1", report(ST_ALERT, LBL_HEAVY_METALS), 10);
    net.updateReport("N2", report(ST_NORMAL, LBL_NONE), 10);
    NetworkAssessment a = net.assess("N1", 10);
    TEST_ASSERT_FALSE(a.hasUpstream);
    TEST_ASSERT_EQUAL(NF_SOURCE_LOCAL_SIDE, a.finding);
}

void test_assess_rain_confirmed_and_unconfirmed() {
    net.updateReport("N0", report(ST_NORMAL, LBL_RAIN, true), 10);
    net.updateReport("N1", report(ST_NORMAL, LBL_RAIN, true), 10);
    net.updateReport("N2", report(ST_NORMAL, LBL_RAIN, true), 10);
    net.updateReport("N3", report(ST_NORMAL, LBL_NONE), 10);
    TEST_ASSERT_EQUAL(NF_RAIN_CONFIRMED, net.assess("N1", 10).finding);

    net.updateReport("N0", report(ST_NORMAL, LBL_NONE), 10);
    net.updateReport("N2", report(ST_NORMAL, LBL_NONE), 10);
    TEST_ASSERT_EQUAL(NF_RAIN_UNCONFIRMED, net.assess("N1", 10).finding);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sorted_insert_and_positions);
    RUN_TEST(test_duplicate_and_capacity);
    RUN_TEST(test_remove_frees_position);
    RUN_TEST(test_divert_keeps_report_and_order);
    RUN_TEST(test_assess_source_on_local_side);
    RUN_TEST(test_assess_cross_section);
    RUN_TEST(test_assess_from_upstream);
    RUN_TEST(test_assess_between_when_no_sibling_data);
    RUN_TEST(test_assess_stale_upstream_ignored);
    RUN_TEST(test_assess_rain_confirmed_and_unconfirmed);
    return UNITY_END();
}
