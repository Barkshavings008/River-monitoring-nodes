// PC tests for the messages sent to the phone: pio test -e native
#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "phone_messages.h"

struct River_network network;
struct Speed_tracker tracker;
char line[BLE_LINE_SIZE];

struct Node_report make_report(const char *id, enum Label_id label) {
    struct Node_report report;
    memset(&report, 0, sizeof(struct Node_report));
    strcpy(report.id, id);
    report.label = label;
    report.state = ST_NORMAL;
    if (label != LBL_NONE) {
        report.state = ST_ALERT;
        report.confidence = 0.85f;
    }
    float now[S_COUNT] = { 5.12f, 412.0f, 31.4f, 18.25f };
    for (int s = 0; s < S_COUNT; s++) {
        report.now[s] = now[s];
        report.now_valid[s] = true;
        report.base[s] = now[s] * 0.9f;
        report.base_valid[s] = true;
    }
    return report;
}

void setUp(void) {
    network_clear(&network);
    network_add_node(&network, "N0", 0, "upstream", false);
    network_add_node(&network, "N1", 1000, "left bank", true);
    speed_tracker_begin(&tracker);
}

void tearDown(void) {
}

void test_status_has_readings_label_and_ends_in_newline(void) {
    struct Node_report report = make_report("N1", LBL_HEAVY_METALS);
    network_update_report(&network, "N1", report, 5);
    struct Network_assessment a = network_assess(&network, "N1", 5);
    int length = build_status_message(line, BLE_LINE_SIZE, report, a, &tracker, 5);
    TEST_ASSERT_EQUAL((int)strlen(line), length);
    TEST_ASSERT_EQUAL_CHAR('\n', line[length - 1]);
    TEST_ASSERT_NOT_NULL(strstr(line, "{\"type\":\"status\",\"node\":\"N1\""));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"now\":{\"ph\":5.12,\"tds\":412,\"ntu\":31.4,\"temp\":18.2"));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"state\":\"ALERT\""));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"code\":\"LIKELY_HEAVY_METALS\""));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"category\":\"POLLUTION\""));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"speed\":null"));
}

void test_missing_reading_is_null(void) {
    struct Node_report report = make_report("N1", LBL_NONE);
    report.now_valid[S_TEMP] = false;
    report.fault[S_TEMP] = F_NO_READING;
    struct Network_assessment a = network_assess(&network, "N1", 5);
    build_status_message(line, BLE_LINE_SIZE, report, a, &tracker, 5);
    TEST_ASSERT_NOT_NULL(strstr(line, "\"temp\":null}"));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"faults\":[{\"sensor\":\"Water temp\""));
}

void test_speed_included_once_known(void) {
    tracker.has_estimate = true;
    strcpy(tracker.latest.from_id, "N0");
    strcpy(tracker.latest.to_id, "N1");
    tracker.latest.label = LBL_SEWAGE;
    tracker.latest.metres = 1000;
    tracker.latest.minutes = 20;
    tracker.latest.metres_per_sec = 0.833f;
    tracker.latest.found_min = 30;
    struct Node_report report = make_report("N1", LBL_SEWAGE);
    struct Network_assessment a = network_assess(&network, "N1", 33);
    build_status_message(line, BLE_LINE_SIZE, report, a, &tracker, 33);
    TEST_ASSERT_NOT_NULL(strstr(line, "\"speed\":{\"mps\":0.83,\"from\":\"N0\",\"to\":\"N1\","
                                      "\"metres\":1000,\"minutes\":20,\"age_min\":3"));
    int length = build_speed_message(line, BLE_LINE_SIZE, tracker.latest, 30);
    TEST_ASSERT_TRUE(length > 0);
    TEST_ASSERT_NOT_NULL(strstr(line, "{\"type\":\"speed\",\"speed\":{\"mps\":0.83"));
}

void test_alert_message(void) {
    struct Alert_event event;
    event.id = LBL_SEWAGE;
    event.on = true;
    event.confidence = 0.9f;
    struct Node_report report = make_report("N1", LBL_SEWAGE);
    build_alert_message(line, BLE_LINE_SIZE, event, report, 12);
    TEST_ASSERT_NOT_NULL(strstr(line, "{\"type\":\"alert\",\"node\":\"N1\",\"min\":12,\"on\":true"));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"name\":\"Likely sewage\""));
}

void test_quotes_in_place_names_are_escaped(void) {
    network_add_node(&network, "N2", 2000, "the \"bend\" \\ weir", false);
    build_nodes_message(line, BLE_LINE_SIZE, &network, 5);
    TEST_ASSERT_NOT_NULL(strstr(line, "\"place\":\"the \\\"bend\\\" \\\\ weir\""));
    // No report yet, so no readings for it
    TEST_ASSERT_NOT_NULL(strstr(line, "\"status\":\"no data\"}"));
}

void test_too_small_buffer_sends_nothing(void) {
    struct Node_report report = make_report("N1", LBL_SEWAGE);
    struct Network_assessment a = network_assess(&network, "N1", 5);
    char small[100];
    TEST_ASSERT_EQUAL(0, build_status_message(small, 100, report, a, &tracker, 5));
    TEST_ASSERT_EQUAL_STRING("", small);
}

// The biggest messages possible still fit in BLE_LINE_SIZE
void test_worst_case_fits(void) {
    struct Node_report report = make_report("N1", LBL_HEAVY_METALS);
    report.num_also = MAX_ALSO;
    for (int i = 0; i < MAX_ALSO; i++) {
        report.also[i] = (enum Label_id)(LBL_INDUSTRIAL + (i % (LBL_FAULT - LBL_INDUSTRIAL)));
    }
    for (int s = 0; s < S_COUNT; s++) {
        report.fault[s] = F_OUT_OF_RANGE;
    }
    tracker.has_estimate = true;
    tracker.latest.label = LBL_INDUSTRIAL;
    network_update_report(&network, "N0", report, 5);
    network_update_report(&network, "N1", report, 5);
    struct Network_assessment a = network_assess(&network, "N1", 5);
    TEST_ASSERT_TRUE(build_status_message(line, BLE_LINE_SIZE, report, a, &tracker, 5) > 0);

    // A full network of nodes with the longest ids and place names
    network_clear(&network);
    char id[NODE_ID_LEN];
    for (int i = 0; i < MAX_NODES; i++) {
        snprintf(id, NODE_ID_LEN, "NODE%03d", i);
        TEST_ASSERT_TRUE(network_add_node(&network, id, 100000 + i, "\"place name long\"", false));
        report.label = LBL_EFFLUENT;
        network_update_report(&network, id, report, 5);
    }
    TEST_ASSERT_TRUE(build_nodes_message(line, BLE_LINE_SIZE, &network, 5) > 0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_status_has_readings_label_and_ends_in_newline);
    RUN_TEST(test_missing_reading_is_null);
    RUN_TEST(test_speed_included_once_known);
    RUN_TEST(test_alert_message);
    RUN_TEST(test_quotes_in_place_names_are_escaped);
    RUN_TEST(test_too_small_buffer_sends_nothing);
    RUN_TEST(test_worst_case_fits);
    return UNITY_END();
}
