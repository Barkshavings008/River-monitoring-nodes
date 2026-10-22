#include "node_link.h"
#include <string.h>

////////////////////////
// Function prototypes//
////////////////////////
uint8_t bits_from(bool values[]);
void bits_to(uint8_t bits, bool values[]);
////////////////////////

/////////////////////////
////// PACKING ////////
/////////////////////////

// Squeezes a report into a packet
void pack_report(struct Node_packet *packet, struct Node_report report, unsigned long distance_m,
    const char *place) {
    memset(packet, 0, sizeof(struct Node_packet));
    packet->magic[0] = PACKET_MAGIC_1;
    packet->magic[1] = PACKET_MAGIC_2;
    packet->version = PACKET_VERSION;
    strncpy(packet->id, report.id, NODE_ID_LEN - 1);
    strncpy(packet->place, place, NODE_PLACE_LEN - 1);
    packet->distance_m = (uint32_t)distance_m;
    packet->time_sec = (uint32_t)report.time_sec;
    packet->state = (uint8_t)report.state;
    packet->label = (uint8_t)report.label;
    packet->num_also = (uint8_t)report.num_also;
    for (int i = 0; i < report.num_also && i < MAX_ALSO; i++) {
        packet->also[i] = (uint8_t)report.also[i];
    }
    for (int s = 0; s < S_COUNT; s++) {
        packet->fault[s] = (uint8_t)report.fault[s];
        packet->now[s] = report.now[s];
        packet->base[s] = report.base[s];
    }
    packet->now_valid_bits = bits_from(report.now_valid);
    packet->base_valid_bits = bits_from(report.base_valid);
    packet->fitted_bits = bits_from(report.fitted);
    if (report.rain_pattern) {
        packet->flags |= 1;
    }
    if (report.ph_recalibrate) {
        packet->flags |= 2;
    }
    packet->confidence = report.confidence;
    packet->learned_minutes = (uint16_t)report.learned_minutes;
    packet->learn_needed = (uint16_t)report.learn_needed;
}

// Turns a received packet back into a report. Returns false for anything
// that isn't one of our packets, or has values that can't be right.
bool unpack_report(const uint8_t *data, int length, struct Node_report *report,
    unsigned long *distance_m, char *place) {
    if (length != (int)sizeof(struct Node_packet)) {
        return false;
    }
    struct Node_packet packet;
    memcpy(&packet, data, sizeof(struct Node_packet));
    if (packet.magic[0] != PACKET_MAGIC_1 || packet.magic[1] != PACKET_MAGIC_2 ||
        packet.version != PACKET_VERSION) {
        return false;
    }
    packet.id[NODE_ID_LEN - 1] = '\0';
    packet.place[NODE_PLACE_LEN - 1] = '\0';
    if (packet.id[0] == '\0' || packet.state > ST_FAULT || packet.label >= LBL_COUNT ||
        packet.num_also > MAX_ALSO) {
        return false;
    }
    for (int i = 0; i < packet.num_also; i++) {
        if (packet.also[i] >= LBL_COUNT) {
            return false;
        }
    }
    for (int s = 0; s < S_COUNT; s++) {
        if (packet.fault[s] > F_TEMP_JUMP) {
            return false;
        }
    }

    memset(report, 0, sizeof(struct Node_report));
    strncpy(report->id, packet.id, NODE_ID_LEN - 1);
    report->time_sec = packet.time_sec;
    report->state = (enum Node_state)packet.state;
    report->label = (enum Label_id)packet.label;
    report->confidence = packet.confidence;
    report->num_also = packet.num_also;
    for (int i = 0; i < packet.num_also; i++) {
        report->also[i] = (enum Label_id)packet.also[i];
    }
    for (int s = 0; s < S_COUNT; s++) {
        report->now[s] = packet.now[s];
        report->base[s] = packet.base[s];
        report->fault[s] = (enum Fault_code)packet.fault[s];
    }
    bits_to(packet.now_valid_bits, report->now_valid);
    bits_to(packet.base_valid_bits, report->base_valid);
    bits_to(packet.fitted_bits, report->fitted);
    report->rain_pattern = (packet.flags & 1) != 0;
    report->ph_recalibrate = (packet.flags & 2) != 0;
    report->learned_minutes = packet.learned_minutes;
    report->learn_needed = packet.learn_needed;
    *distance_m = packet.distance_m;
    strncpy(place, packet.place, NODE_PLACE_LEN - 1);
    place[NODE_PLACE_LEN - 1] = '\0';
    return true;
}

// Saves a received report into the river map. A node that isn't on the map
// yet is added (on the main river, at the distance in the packet), and its
// id is copied into added_id. Our own packets coming back are ignored.
// Returns true if the report was saved.
bool network_take_packet(struct River_network *network, const uint8_t *data, int length,
    const char *local_id, unsigned long now_min, char *added_id) {
    struct Node_report report;
    unsigned long distance_m;
    char place[NODE_PLACE_LEN];
    added_id[0] = '\0';
    if (!unpack_report(data, length, &report, &distance_m, place)) {
        return false;
    }
    if (strncmp(report.id, local_id, NODE_ID_LEN) == 0) {
        return false;
    }
    if (network_find_node(network, report.id) < 0) {
        if (!network_add_node(network, report.id, distance_m, place, false)) {
            return false;   // map full
        }
        strncpy(added_id, report.id, NODE_ID_LEN);
    }
    return network_update_report(network, report.id, report, now_min);
}

/////////////////////////
// Function definitions//
/////////////////////////

uint8_t bits_from(bool values[]) {
    uint8_t bits = 0;
    for (int s = 0; s < S_COUNT; s++) {
        if (values[s]) {
            bits |= (uint8_t)(1 << s);
        }
    }
    return bits;
}

void bits_to(uint8_t bits, bool values[]) {
    for (int s = 0; s < S_COUNT; s++) {
        values[s] = (bits & (1 << s)) != 0;
    }
}
