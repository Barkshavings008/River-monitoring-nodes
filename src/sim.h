#pragma once
// SIMULATE mode: scripted fake readings (normal -> rain -> normal -> acid
// spike), plus fake neighbour nodes that run the same engine as the real node.
#include "types.h"
#include "node_engine.h"
#include "network.h"

// Roles for the simulated nodes
#define SIM_LOCAL 'L'
#define SIM_UPSTREAM 'U'
#define SIM_SIBLING 'S'
#define SIM_DOWNSTREAM 'D'

// The script, in minutes within a repeating cycle. Even cycles: rain at every
// node (confirmed). Odd cycles: rain only at this node (unconfirmed).
#define SIM_CYCLE_MIN 45
#define SIM_RAIN_START 8
#define SIM_RAIN_END 15
#define SIM_ACID_START 20
#define SIM_ACID_END 33
#define SIM_DOWNSTREAM_DELAY 2
#define SIM_SAMPLES_PER_MIN 5

#define MAX_SIM_NODES 4

////////////////////////
// Function prototypes//
////////////////////////
struct Reading sim_reading(char role, unsigned long minute, unsigned long sample);
void sim_add_node(const char *id, char role);
void sim_tick(unsigned long sim_minute, unsigned long uptime_sec, struct River_network *network,
    unsigned long now_min);
////////////////////////
