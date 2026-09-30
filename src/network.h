#pragma once
// The river network is an array of nodes, kept in order of distance down the
// river. Nodes at the same distance share a "position" (e.g. the two banks at
// one point) and get compared with each other.
//
//   index:   0        1          2           3
//   node:    N0       N1         N2          N3
//   dist:    0 m      1000 m     1000 m      2500 m
//            [pos 0]  [------ pos 1 ------]  [pos 2]
//
// Adding, removing and moving nodes just shifts the array along.
#include "types.h"

struct River_node {
    char id[NODE_ID_LEN];
    char place[NODE_PLACE_LEN];
    unsigned long distance_m;      // distance down the river from the start point
    bool is_local;                 // true = this board
    bool has_report;
    unsigned long last_update_min;
    struct Node_report report;     // the node's latest report
};

struct River_network {
    struct River_node nodes[MAX_NODES];
    int num_nodes;
};

enum Net_finding {
    NF_NONE,                  // nothing to compare / nothing flagged
    NF_NO_PEERS,              // flagged, but no fresh data from any other node
    NF_SOURCE_LOCAL_SIDE,     // alert here, other nodes at this position are normal
    NF_SOURCE_CROSS_SECTION,  // alert here and at every other node at this position
    NF_SOURCE_BETWEEN,        // alert here, nearest upstream position is normal
    NF_FROM_UPSTREAM,         // same label at the nearest upstream position
    NF_RAIN_CONFIRMED,        // rain pattern at most nodes
    NF_RAIN_UNCONFIRMED,      // rain pattern only here: maybe an outfall or leak
};

// What comparing this node with the other nodes found
struct Network_assessment {
    enum Net_finding finding;
    char local_id[NODE_ID_LEN];
    unsigned long local_distance;
    bool has_upstream;
    char upstream_id[NODE_ID_LEN];
    unsigned long upstream_distance;
    int siblings;            // fresh nodes at the same position (not counting this one)
    int siblings_alerting;
    int peers;               // fresh nodes anywhere (not counting this one)
    int rain_peers;
};

////////////////////////
// Function prototypes//
////////////////////////
void network_clear(struct River_network *network);
bool network_add_node(struct River_network *network, const char *id, unsigned long distance_m,
    const char *place, bool is_local);
bool network_remove_node(struct River_network *network, const char *id);
bool network_move_node(struct River_network *network, const char *id, unsigned long new_distance_m);
int network_find_node(struct River_network *network, const char *id);
bool network_update_report(struct River_network *network, const char *id,
    struct Node_report report, unsigned long now_min);
bool node_is_fresh(struct River_node *node, unsigned long now_min);
int network_position_count(struct River_network *network);
unsigned long network_position_distance(struct River_network *network, int position);
int network_nodes_at(struct River_network *network, unsigned long distance_m, int indexes[], int max);
struct Network_assessment network_assess(struct River_network *network, const char *local_id,
    unsigned long now_min);
////////////////////////
