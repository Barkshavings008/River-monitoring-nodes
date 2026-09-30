#pragma once
// The river network is an array of nodes. Nodes sit on "branches" (separate
// streams or rivers), and a branch can flow into another one, e.g. two
// streams joining to make the main river:
//
//   stream_a  N0 ----.
//                     >---- N2 ---- N3 ----  main
//   stream_b  N1 ----'
//
// A node's distance is measured down its own branch from the branch's top
// end. Nodes on the same branch at the same distance share a "position"
// (e.g. the two banks at one point) and get compared with each other.
//
// With only one branch it works like a single straight river:
//
//   index:   0        1          2           3
//   node:    N0       N1         N2          N3
//   dist:    0 m      1000 m     1000 m      2500 m
//            [pos 0]  [------ pos 1 ------]  [pos 2]
//
// The array is kept in order from upstream to downstream (streams before the
// river they flow into, then by distance). Adding, removing and moving nodes
// just shifts the array along.
#include "types.h"

struct River_branch {
    char name[BRANCH_NAME_LEN];
    unsigned long length_m;    // from its top end down to where it joins another branch
    int flows_into;            // index of the branch this one joins (-1 = doesn't join anything)
    unsigned long joins_at_m;  // distance down that branch where it joins
};

struct River_node {
    char id[NODE_ID_LEN];
    char place[NODE_PLACE_LEN];
    int branch;                    // index into the network's branches
    unsigned long distance_m;      // distance down its branch from the top end
    bool is_local;                 // true = this board
    bool has_report;
    unsigned long last_update_min;
    struct Node_report report;     // the node's latest report
};

struct River_network {
    struct River_node nodes[MAX_NODES];
    int num_nodes;
    struct River_branch branches[MAX_BRANCHES];
    int num_branches;
};

enum Net_finding {
    NF_NONE,                  // nothing to compare / nothing flagged
    NF_NO_PEERS,              // flagged, but no fresh data from any other node
    NF_SOURCE_LOCAL_SIDE,     // alert here, other nodes at this position are normal
    NF_SOURCE_CROSS_SECTION,  // alert here and at every other node at this position
    NF_SOURCE_BETWEEN,        // alert here, nearest upstream node(s) are normal
    NF_FROM_UPSTREAM,         // same label at a nearest upstream node
    NF_RAIN_CONFIRMED,        // rain pattern at most nodes
    NF_RAIN_UNCONFIRMED,      // rain pattern only here: maybe an outfall or leak
    NF_NO_UPSTREAM,           // alert here, other nodes are fine but none are upstream of this one
};

#define UPSTREAM_LIST_LEN 40

// What comparing this node with the other nodes found
struct Network_assessment {
    enum Net_finding finding;
    char local_id[NODE_ID_LEN];
    unsigned long local_distance;
    char local_branch[BRANCH_NAME_LEN];
    bool branched;           // true if the network has more than one branch
    bool has_upstream;
    char upstream_id[NODE_ID_LEN];       // the upstream node the finding is about
    unsigned long upstream_distance;
    char upstream_branch[BRANCH_NAME_LEN];
    long upstream_gap_m;                 // river distance from that node down to this one
    int num_upstream;                    // nearest fresh upstream nodes (one per stream)
    char upstream_list[UPSTREAM_LIST_LEN]; // their ids, e.g. "N0, N1"
    int siblings;            // fresh nodes at the same position (not counting this one)
    int siblings_alerting;
    int peers;               // fresh nodes anywhere (not counting this one)
    int rain_peers;
};

////////////////////////
// Function prototypes//
////////////////////////
void network_clear(struct River_network *network);

// Branches: add the river a stream flows into before adding the stream.
// flows_into = NULL or "" for a branch that doesn't join anything.
int network_add_branch(struct River_network *network, const char *name, unsigned long length_m,
    const char *flows_into, unsigned long joins_at_m);
int network_find_branch(struct River_network *network, const char *name);

// network_add_node() puts the node on the MAIN_BRANCH (made automatically)
bool network_add_node(struct River_network *network, const char *id, unsigned long distance_m,
    const char *place, bool is_local);
bool network_add_node_on_branch(struct River_network *network, const char *id, const char *branch,
    unsigned long distance_m, const char *place, bool is_local);
bool network_remove_node(struct River_network *network, const char *id);
bool network_move_node(struct River_network *network, const char *id, unsigned long new_distance_m);
bool network_move_node_to_branch(struct River_network *network, const char *id, const char *branch,
    unsigned long new_distance_m);
int network_find_node(struct River_network *network, const char *id);
bool network_update_report(struct River_network *network, const char *id,
    struct Node_report report, unsigned long now_min);
bool node_is_fresh(struct River_node *node, unsigned long now_min);
long network_river_distance(struct River_network *network, int from_index, int to_index);

int network_position_count(struct River_network *network);
unsigned long network_position_distance(struct River_network *network, int position);
int network_position_branch(struct River_network *network, int position);
int network_nodes_at(struct River_network *network, int branch, unsigned long distance_m,
    int indexes[], int max);

struct Network_assessment network_assess(struct River_network *network, const char *local_id,
    unsigned long now_min);
////////////////////////
