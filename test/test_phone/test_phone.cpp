// PC tests for the messages sent to the phone: pio test -e native
#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "phone_messages.h"
#include "phone_alerts.h"

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
    int length = build_status_message(line, BLE_LINE_SIZE, report, a, &tracker, 5, "off", false);
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
    build_status_message(line, BLE_LINE_SIZE, report, a, &tracker, 5, "off", false);
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
    build_status_message(line, BLE_LINE_SIZE, report, a, &tracker, 33, "off", false);
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
    TEST_ASSERT_EQUAL(0, build_status_message(small, 100, report, a, &tracker, 5, "off", false));
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
    TEST_ASSERT_TRUE(build_status_message(line, BLE_LINE_SIZE, report, a, &tracker, 5, "off", false) > 0);

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

// ---------- WiFi (ntfy) notifications ----------

void test_ntfy_alert_has_title_readings_and_details(void) {
    struct Node_report report = make_report("N1", LBL_HEAVY_METALS);
    network_update_report(&network, "N1", report, 5);
    struct Network_assessment a = network_assess(&network, "N1", 5);
    struct Alert_event events[3];
    events[0].id = LBL_INDUSTRIAL;      // pollution labels keep the order they turned on in
    events[0].on = true;
    events[0].confidence = 0.67f;
    events[1].id = LBL_HEAVY_METALS;
    events[1].on = true;
    events[1].confidence = 0.77f;
    events[2].id = LBL_RAIN;            // rain isn't an alert
    events[2].on = true;
    events[2].confidence = 1.0f;
    char out[NOTIFY_SIZE];
    int length = build_ntfy_alert(out, NOTIFY_SIZE, "topic-x", events, 3, report, a, &tracker, 5);
    TEST_ASSERT_TRUE(length > 0);
    TEST_ASSERT_NOT_NULL(strstr(out, "{\"topic\":\"topic-x\",\"title\":\"POLLUTION ALERT at N1\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "Likely industrial discharge (67% sure)"));
    TEST_ASSERT_NOT_NULL(strstr(out, "Also: Likely heavy metals / acid mine drainage (77% sure)"));
    TEST_ASSERT_NOT_NULL(strstr(out, "pH 5.12 (normal 4.61)"));
    TEST_ASSERT_NOT_NULL(strstr(out, "Possible pollutants:"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"priority\":5,\"tags\":[\"rotating_light\"]}"));
    TEST_ASSERT_NULL(strstr(out, "Likely rain"));
}

void test_ntfy_watch_alert_and_nothing_to_send(void) {
    struct Node_report report = make_report("N1", LBL_THERMAL);
    struct Network_assessment a = network_assess(&network, "N1", 5);
    struct Alert_event event;
    event.id = LBL_THERMAL;
    event.on = true;
    event.confidence = 1.0f;
    char out[NOTIFY_SIZE];
    TEST_ASSERT_TRUE(build_ntfy_alert(out, NOTIFY_SIZE, "t", &event, 1, report, a, &tracker, 5) > 0);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"title\":\"Watch at N1\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"priority\":4"));
    // An alert turning off doesn't send anything
    event.on = false;
    TEST_ASSERT_EQUAL(0, build_ntfy_alert(out, NOTIFY_SIZE, "t", &event, 1, report, a, &tracker, 5));
}

void test_ntfy_speed_said_once_per_pair(void) {
    struct Speed_estimate speeds[2];
    memset(speeds, 0, sizeof(speeds));
    for (int i = 0; i < 2; i++) {
        strcpy(speeds[i].from_id, "N0");
        strcpy(speeds[i].to_id, "N1");
        speeds[i].metres = 1000;
        speeds[i].minutes = 20;
        speeds[i].metres_per_sec = 0.833f;
    }
    speeds[0].label = LBL_HEAVY_METALS;
    speeds[1].label = LBL_INDUSTRIAL;
    char out[NOTIFY_SIZE];
    TEST_ASSERT_TRUE(build_ntfy_speed(out, NOTIFY_SIZE, "t", speeds, 2) > 0);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"title\":\"Water speed: about 0.83 m/s\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "N0 to N1: 1000 m in 20 min"));
    TEST_ASSERT_NULL(strstr(out, "industrial"));   // same pair, only said once
    TEST_ASSERT_EQUAL(0, build_ntfy_speed(out, NOTIFY_SIZE, "t", speeds, 0));
}

void test_ntfy_fault_message(void) {
    struct Node_report report = make_report("N1", LBL_NONE);
    report.fault[S_TEMP] = F_NO_READING;
    char out[NOTIFY_SIZE];
    TEST_ASSERT_TRUE(build_ntfy_fault(out, NOTIFY_SIZE, "t", report, S_TEMP) > 0);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"title\":\"Sensor problem at N1: Water temp\""));
    TEST_ASSERT_TRUE(build_ntfy_fault(out, NOTIFY_SIZE, "t", report, S_COUNT) > 0);
    TEST_ASSERT_NOT_NULL(strstr(out, "pH probe drifting"));
}

// A sensor problem is only reported once it has lasted FAULT_NOTIFY_MIN
// minutes, and only once until it's fixed
void test_fault_watch_waits_and_reports_once(void) {
    struct Fault_watch watch;
    fault_watch_reset(&watch);
    struct Node_report report = make_report("N1", LBL_NONE);
    int sensors[S_COUNT + 1];
    report.fault[S_TEMP] = F_NO_READING;
    for (int m = 1; m < FAULT_NOTIFY_MIN; m++) {
        TEST_ASSERT_EQUAL(0, fault_watch_update(&watch, report, sensors, S_COUNT + 1));
    }
    TEST_ASSERT_EQUAL(1, fault_watch_update(&watch, report, sensors, S_COUNT + 1));
    TEST_ASSERT_EQUAL(S_TEMP, sensors[0]);
    for (int m = 0; m < 10; m++) {
        TEST_ASSERT_EQUAL(0, fault_watch_update(&watch, report, sensors, S_COUNT + 1));
    }
    // A one-minute glitch never gets reported
    report.fault[S_TEMP] = F_NONE;
    fault_watch_update(&watch, report, sensors, S_COUNT + 1);
    report.fault[S_PH] = F_OUT_OF_RANGE;
    TEST_ASSERT_EQUAL(0, fault_watch_update(&watch, report, sensors, S_COUNT + 1));
    report.fault[S_PH] = F_NONE;
    TEST_ASSERT_EQUAL(0, fault_watch_update(&watch, report, sensors, S_COUNT + 1));
    // Fixed then broken again: reported again
    report.fault[S_TEMP] = F_NO_READING;
    report.ph_recalibrate = true;
    for (int m = 1; m < FAULT_NOTIFY_MIN; m++) {
        fault_watch_update(&watch, report, sensors, S_COUNT + 1);
    }
    TEST_ASSERT_EQUAL(2, fault_watch_update(&watch, report, sensors, S_COUNT + 1));
    TEST_ASSERT_EQUAL(S_TEMP, sensors[0]);
    TEST_ASSERT_EQUAL(S_COUNT, sensors[1]);
}

void test_status_has_wifi_and_armed(void) {
    struct Node_report report = make_report("N1", LBL_NONE);
    struct Network_assessment a = network_assess(&network, "N1", 5);
    build_status_message(line, BLE_LINE_SIZE, report, a, &tracker, 5, "connected", true);
    TEST_ASSERT_NOT_NULL(strstr(line, "\"wifi\":\"connected\",\"armed\":true"));
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
    RUN_TEST(test_ntfy_alert_has_title_readings_and_details);
    RUN_TEST(test_ntfy_watch_alert_and_nothing_to_send);
    RUN_TEST(test_ntfy_speed_said_once_per_pair);
    RUN_TEST(test_ntfy_fault_message);
    RUN_TEST(test_fault_watch_waits_and_reports_once);
    RUN_TEST(test_status_has_wifi_and_armed);
    return UNITY_END();
}
