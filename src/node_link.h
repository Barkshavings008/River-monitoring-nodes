#pragma once
// Shares reports between real boards over ESP-NOW, the ESP32's built-in
// board-to-board radio (no router or internet needed between boards).
//
// Every minute each board broadcasts its report in a small packet. A board
// that hears a node it doesn't know yet adds it to its river map at the
// distance the packet says, so each board only needs to know its own place.
//
//   N0 (turbidity)  ~~~radio~~~  N1 (all 4 sensors)  ~~~radio~~~  N2 (turbidity)
//
// ESP-NOW has to be on the same WiFi channel on every board. With WiFi
// notifications on, that happens by itself as long as every board joins the
// same hotspot. Boards without WiFi use ESPNOW_CHANNEL.
//
// The packing below has no Arduino code, so it's tested on a PC.
#include <stdint.h>
#include "types.h"
#include "network.h"

#define PACKET_MAGIC_1 'R'
#define PACKET_MAGIC_2 'N'
#define PACKET_VERSION 1

// One node's report, squeezed into well under ESP-NOW's 250 byte limit
struct __attribute__((packed)) Node_packet {
    uint8_t magic[2];        // "RN", so packets from other ESP-NOW gadgets are ignored
    uint8_t version;
    char id[NODE_ID_LEN];
    char place[NODE_PLACE_LEN];
    uint32_t distance_m;
    uint32_t time_sec;
    uint8_t state;
    uint8_t label;
    uint8_t num_also;
    uint8_t also[MAX_ALSO];
    uint8_t fault[S_COUNT];
    uint8_t now_valid_bits;  // bit s = sensor s has a reading
    uint8_t base_valid_bits;
    uint8_t fitted_bits;
    uint8_t flags;           // bit 0 = rain pattern, bit 1 = pH needs recalibrating
    float confidence;
    float now[S_COUNT];
    float base[S_COUNT];
    uint16_t learned_minutes;
    uint16_t learn_needed;
};

////////////////////////
// Function prototypes//
////////////////////////
void pack_report(struct Node_packet *packet, struct Node_report report, unsigned long distance_m,
    const char *place);
bool unpack_report(const uint8_t *data, int length, struct Node_report *report,
    unsigned long *distance_m, char *place);
bool network_take_packet(struct River_network *network, const uint8_t *data, int length,
    const char *local_id, unsigned long now_min, char *added_id);

void node_link_begin(void);
void node_link_send(struct Node_report report);
bool node_link_receive(uint8_t *data, int *length);
////////////////////////
