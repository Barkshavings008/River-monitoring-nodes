#include "sim.h"
#include <string.h>

#define MAX_SIM_EVENTS 4

// Clean water readings for one simulated node
struct Sim_profile {
    float ph;
    float tds;
    float ntu;
    float temp;
};

// One simulated neighbour node
struct Sim_node {
    char id[NODE_ID_LEN];
    char role;
    struct Node_engine engine;
};

struct Sim_node sim_nodes[MAX_SIM_NODES];
int num_sim_nodes = 0;

////////////////////////
// Function prototypes//
////////////////////////
struct Sim_profile normal_for(char role);
float noise(uint32_t a, uint32_t b, uint32_t c);
////////////////////////

// One fake sample for a node with the given role
struct Reading sim_reading(char role, unsigned long minute, unsigned long sample) {
    struct Sim_profile p = normal_for(role);
    unsigned long cycle = minute / SIM_CYCLE_MIN;
    unsigned long c = minute % SIM_CYCLE_MIN;   // minute within this cycle

    // Rain: every node on even cycles, only this node on odd cycles
    bool rain_time = c >= SIM_RAIN_START && c < SIM_RAIN_END;
    if (rain_time && (role == SIM_LOCAL || cycle % 2 == 0)) {
        p.tds *= 0.80f;
        p.ntu = 120.0f;   // muddy storm water
        p.temp -= 0.8f;
    }

    // Acid spike at this node
    if (role == SIM_LOCAL && c >= SIM_ACID_START && c < SIM_ACID_END) {
        p.ph = 4.6f;
        p.tds = 612.0f;
        p.ntu = 18.2f;
        p.temp += 0.3f;
    }

    // Watered down plume reaching the downstream node a bit later
    if (role == SIM_DOWNSTREAM && c >= SIM_ACID_START + SIM_DOWNSTREAM_DELAY &&
        c < SIM_ACID_END + SIM_DOWNSTREAM_DELAY) {
        p.ph = 5.6f;
        p.tds = 400.0f;
        p.ntu = 12.0f;
    }

    uint32_t seed = minute * 64 + sample;
    struct Reading reading;
    reading.value[S_PH] = p.ph + 0.03f * noise(seed, role, 1);
    reading.value[S_TDS] = p.tds * (1.0f + 0.015f * noise(seed, role, 2));
    reading.value[S_NTU] = p.ntu * (1.0f + 0.03f * noise(seed, role, 3));
    reading.value[S_TEMP] = p.temp + 0.04f * noise(seed, role, 4);
    for (int s = 0; s < S_COUNT; s++) {
        reading.valid[s] = true;
    }
    return reading;
}

// Adds a simulated node (called from setup_river_layout() in main.cpp)
void sim_add_node(const char *id, char role) {
    if (num_sim_nodes >= MAX_SIM_NODES) {
        return;
    }
    struct Sim_node *node = &sim_nodes[num_sim_nodes];
    num_sim_nodes++;
    strncpy(node->id, id, NODE_ID_LEN - 1);
    node->id[NODE_ID_LEN - 1] = '\0';
    node->role = role;
    engine_begin(&node->engine, id);
}

// Runs one minute for every simulated node and gives their reports to the network
void sim_tick(unsigned long sim_minute, unsigned long uptime_sec, struct River_network *network,
    unsigned long now_min) {
    for (int i = 0; i < num_sim_nodes; i++) {
        struct Sim_node *node = &sim_nodes[i];
        // Skip nodes that were deleted from the network
        if (network_find_node(network, node->id) >= 0) {
            for (int k = 0; k < SIM_SAMPLES_PER_MIN; k++) {
                engine_add_sample(&node->engine, sim_reading(node->role, sim_minute, k));
            }
            struct Node_report report;
            struct Alert_event events[MAX_SIM_EVENTS];
            engine_close_minute(&node->engine, uptime_sec, &report, events, MAX_SIM_EVENTS);
            network_update_report(network, node->id, report, now_min);
        }
    }
}

/////////////////////////
// Function definitions//
/////////////////////////

struct Sim_profile normal_for(char role) {
    struct Sim_profile p;
    if (role == SIM_UPSTREAM) {
        p.ph = 7.30f;
        p.tds = 200.0f;
        p.ntu = 7.0f;
        p.temp = 17.0f;
    } else if (role == SIM_SIBLING) {
        p.ph = 7.10f;
        p.tds = 215.0f;
        p.ntu = 8.5f;
        p.temp = 17.2f;
    } else if (role == SIM_DOWNSTREAM) {
        p.ph = 7.20f;
        p.tds = 220.0f;
        p.ntu = 9.0f;
        p.temp = 17.3f;
    } else {
        p.ph = 7.20f;
        p.tds = 210.0f;
        p.ntu = 8.1f;
        p.temp = 17.1f;
    }
    return p;
}

// Random-looking noise between -1 and 1, but the same every run so the demo
// (and the tests) always do the same thing. Has to use uint32_t so the
// multiplying wraps around the same way on the ESP32 and on a PC.
float noise(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t x = (a * 73856093u) ^ (b * 19349663u) ^ (c * 83492791u);
    x ^= x >> 13;
    x *= 0x5bd1e995u;
    x ^= x >> 15;
    return (x & 0xFFFF) / 32767.5f - 1.0f;
}
