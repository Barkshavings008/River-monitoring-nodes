// PC tests for the board-to-board packets: pio test -e native
#include <unity.h>
#include <string.h>
#include "node_link.h"

struct River_network network;

struct Node_report make_report(const char *id) {
    struct Node_report report;
    memset(&report, 0, sizeof(struct Node_report));
    strcpy(report.id, id);
    report.time_sec = 1234;
    report.state = ST_ALERT;
    report.label = LBL_TURBIDITY;
    report.confidence = 0.5f;
    report.num_also = 1;
    report.also[0] = LBL_RAIN;
    float now[S_COUNT] = { 0.0f, 0.0f, 152.5f, 0.0f };
    for (int s = 0; s < S_COUNT; s++) {
        report.now[s] = now[s];
        report.base[s] = now[s] / 10.0f;
    }
    report.now_valid[S_NTU] = true;
    report.base_valid[S_NTU] = true;
    report.fitted[S_NTU] = true;
    report.fault[S_PH] = F_NONE;
    report.rain_pattern = true;
    report.learned_minutes = 12;
    report.learn_needed = 5;
    return report;
}

void setUp(void) {
    network_clear(&network);
    network_add_node(&network, "N1", 1000, "middle", true);
}

void tearDown(void) {
}

void test_packet_fits_espnow(void) {
    TEST_ASSERT_TRUE(sizeof(struct Node_packet) <= 250);
}

void test_round_trip(void) {
    struct Node_packet packet;
    pack_report(&packet, make_report("N2"), 2000, "downstream");
    struct Node_report out;
    unsigned long distance = 0;
    char place[NODE_PLACE_LEN];
    TEST_ASSERT_TRUE(unpack_report((const uint8_t *)&packet, sizeof(packet), &out, &distance, place));
    TEST_ASSERT_EQUAL_STRING("N2", out.id);
    TEST_ASSERT_EQUAL_STRING("downstream", place);
    TEST_ASSERT_EQUAL_UINT32(2000, distance);
    TEST_ASSERT_EQUAL(ST_ALERT, out.state);
    TEST_ASSERT_EQUAL(LBL_TURBIDITY, out.label);
    TEST_ASSERT_EQUAL(1, out.num_also);
    TEST_ASSERT_EQUAL(LBL_RAIN, out.also[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 152.5f, out.now[S_NTU]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.25f, out.base[S_NTU]);
    TEST_ASSERT_TRUE(out.fitted[S_NTU]);
    TEST_ASSERT_FALSE(out.fitted[S_PH]);
    TEST_ASSERT_TRUE(out.now_valid[S_NTU]);
    TEST_ASSERT_FALSE(out.now_valid[S_TDS]);
    TEST_ASSERT_TRUE(out.rain_pattern);
    TEST_ASSERT_FALSE(out.ph_recalibrate);
    TEST_ASSERT_EQUAL(12, out.learned_minutes);
    TEST_ASSERT_EQUAL_UINT32(1234, out.time_sec);
}

void test_bad_packets_refused(void) {
    struct Node_packet packet;
    struct Node_report out;
    unsigned long distance;
    char place[NODE_PLACE_LEN];
    pack_report(&packet, make_report("N2"), 2000, "downstream");
    // Wrong length (another ESP-NOW gadget)
    TEST_ASSERT_FALSE(unpack_report((const uint8_t *)&packet, sizeof(packet) - 1, &out, &distance, place));
    // Wrong magic
    struct Node_packet bad = packet;
    bad.magic[0] = 'X';
    TEST_ASSERT_FALSE(unpack_report((const uint8_t *)&bad, sizeof(bad), &out, &distance, place));
    // A label number that doesn't exist
    bad = packet;
    bad.label = 200;
    TEST_ASSERT_FALSE(unpack_report((const uint8_t *)&bad, sizeof(bad), &out, &distance, place));
    bad = packet;
    bad.num_also = MAX_ALSO + 1;
    TEST_ASSERT_FALSE(unpack_report((const uint8_t *)&bad, sizeof(bad), &out, &distance, place));
    // An id with no end is cut short safely
    bad = packet;
    memset(bad.id, 'Z', NODE_ID_LEN);
    TEST_ASSERT_TRUE(unpack_report((const uint8_t *)&bad, sizeof(bad), &out, &distance, place));
    TEST_ASSERT_EQUAL(NODE_ID_LEN - 1, (int)strlen(out.id));
}

// A new board is added to the map where it says it is; our own packets are ignored
void test_network_take_packet(void) {
    struct Node_packet packet;
    char added[NODE_ID_LEN];
    pack_report(&packet, make_report("N2"), 2000, "downstream");
    TEST_ASSERT_TRUE(network_take_packet(&network, (const uint8_t *)&packet, sizeof(packet), "N1", 7, added));
    TEST_ASSERT_EQUAL_STRING("N2", added);
    int index = network_find_node(&network, "N2");
    TEST_ASSERT_TRUE(index >= 0);
    TEST_ASSERT_EQUAL_UINT32(2000, network.nodes[index].distance_m);
    TEST_ASSERT_EQUAL_STRING("downstream", network.nodes[index].place);
    TEST_ASSERT_TRUE(node_is_fresh(&network.nodes[index], 7));
    TEST_ASSERT_EQUAL(LBL_TURBIDITY, network.nodes[index].report.label);

    // The next one just updates it
    TEST_ASSERT_TRUE(network_take_packet(&network, (const uint8_t *)&packet, sizeof(packet), "N1", 8, added));
    TEST_ASSERT_EQUAL_STRING("", added);
    TEST_ASSERT_EQUAL(2, network.num_nodes);

    // Our own report coming back doesn't overwrite this board
    pack_report(&packet, make_report("N1"), 5, "echo");
    TEST_ASSERT_FALSE(network_take_packet(&network, (const uint8_t *)&packet, sizeof(packet), "N1", 9, added));
    TEST_ASSERT_EQUAL_UINT32(1000, network.nodes[network_find_node(&network, "N1")].distance_m);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_packet_fits_espnow);
    RUN_TEST(test_round_trip);
    RUN_TEST(test_bad_packets_refused);
    RUN_TEST(test_network_take_packet);
    return UNITY_END();
}
