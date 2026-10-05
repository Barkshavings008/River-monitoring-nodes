#include "network.h"
#include <string.h>

#define FAR_DOWNSTREAM 0xFFFFFFFFUL // "below every node" when searching a whole stream

////////////////////////
// Function prototypes//
////////////////////////
void copy_string(char *destination, const char *source, int size);
bool insert_sorted(struct River_network *network, struct River_node node);
bool comes_before(struct River_network *network, struct River_node *a, struct River_node *b);
int branch_depth(struct River_network *network, int branch);
bool same_position(struct River_node *a, struct River_node *b);
void find_upstream_nodes(struct River_network *network, int branch, unsigned long below_m,
    unsigned long now_min, int found[], int *num_found, int depth);
////////////////////////

void network_clear(struct River_network *network) {
    network->num_nodes = 0;
    network->num_branches = 0;
}

/////////////////////////
//////// BRANCHES ///////
/////////////////////////

// Adds a stream/river. length_m is how long it is from its top end down to
// where it joins flows_into (at joins_at_m down that branch). The branch it
// flows into has to be added first. Returns the new branch's index, or -1 if
// the name is empty or used, flows_into doesn't exist, or there's no room.
int network_add_branch(struct River_network *network, const char *name, unsigned long length_m,
    const char *flows_into, unsigned long joins_at_m) {
    if (name == NULL || name[0] == '\0' || network_find_branch(network, name) >= 0) {
        return -1;
    }
    if (strlen(name) > BRANCH_NAME_LEN - 1) {
        return -1; // too long: cutting it short could make two branches look the same
    }
    if (network->num_branches >= MAX_BRANCHES) {
        return -1;
    }
    int parent = -1;
    if (flows_into != NULL && flows_into[0] != '\0') {
        parent = network_find_branch(network, flows_into);
        if (parent < 0) {
            return -1;
        }
    }

    struct River_branch *branch = &network->branches[network->num_branches];
    copy_string(branch->name, name, BRANCH_NAME_LEN);
    branch->length_m = length_m;
    branch->flows_into = parent;
    branch->joins_at_m = joins_at_m;
    network->num_branches++;
    return network->num_branches - 1;
}

// Returns the branch's index, or -1 if there isn't one with that name
int network_find_branch(struct River_network *network, const char *name) {
    if (name == NULL) {
        return -1;
    }
    for (int i = 0; i < network->num_branches; i++) {
        if (strncmp(network->branches[i].name, name, BRANCH_NAME_LEN) == 0) {
            return i;
        }
    }
    return -1;
}

/////////////////////////
///////// NODES /////////
/////////////////////////

// Adds a node on the main branch (the main branch gets made if it isn't there yet)
bool network_add_node(struct River_network *network, const char *id, unsigned long distance_m,
    const char *place, bool is_local) {
    if (network_find_branch(network, MAIN_BRANCH) < 0) {
        network_add_branch(network, MAIN_BRANCH, 0, NULL, 0);
    }
    return network_add_node_on_branch(network, id, MAIN_BRANCH, distance_m, place, is_local);
}

// Adds a node in upstream -> downstream order (after any nodes already at
// that spot). Fails if the id is empty, too long (over NODE_ID_LEN - 1
// characters) or already used, the branch doesn't exist, or the network is full.
bool network_add_node_on_branch(struct River_network *network, const char *id, const char *branch,
    unsigned long distance_m, const char *place, bool is_local) {
    if (id == NULL || id[0] == '\0') {
        return false;
    }
    if (strlen(id) > NODE_ID_LEN - 1) {
        return false; // too long: cutting it short could make two nodes have the same id
    }
    if (network_find_node(network, id) >= 0 || network->num_nodes >= MAX_NODES) {
        return false;
    }
    int branch_index = network_find_branch(network, branch);
    if (branch_index < 0) {
        return false;
    }
    struct River_node node;
    memset(&node, 0, sizeof(struct River_node)); // sets every value in the struct to 0
    copy_string(node.id, id, NODE_ID_LEN);
    copy_string(node.place, place, NODE_PLACE_LEN);
    node.branch = branch_index;
    node.distance_m = distance_m;
    node.is_local = is_local;
    return insert_sorted(network, node);
}

bool network_remove_node(struct River_network *network, const char *id) {
    int index = network_find_node(network, id);
    if (index < 0) {
        return false;
    }
    // Shift everything after it one spot to the left
    for (int j = index; j + 1 < network->num_nodes; j++) {
        network->nodes[j] = network->nodes[j + 1];
    }
    network->num_nodes--;
    return true;
}

// Moves a node to a new distance on the same branch, keeping its latest report
bool network_move_node(struct River_network *network, const char *id, unsigned long new_distance_m) {
    int index = network_find_node(network, id);
    if (index < 0) {
        return false;
    }
    struct River_node node = network->nodes[index];
    network_remove_node(network, id);
    node.distance_m = new_distance_m;
    return insert_sorted(network, node);
}

// Moves a node to a spot on a (possibly different) branch, keeping its latest report
bool network_move_node_to_branch(struct River_network *network, const char *id, const char *branch,
    unsigned long new_distance_m) {
    int index = network_find_node(network, id);
    int branch_index = network_find_branch(network, branch);
    if (index < 0 || branch_index < 0) {
        return false;
    }
    struct River_node node = network->nodes[index];
    network_remove_node(network, id);
    node.branch = branch_index;
    node.distance_m = new_distance_m;
    return insert_sorted(network, node);
}

// Returns where the node is in the array, or -1 if it isn't there
int network_find_node(struct River_network *network, const char *id) {
    if (id == NULL) {
        return -1;
    }
    for (int i = 0; i < network->num_nodes; i++) {
        if (strncmp(network->nodes[i].id, id, NODE_ID_LEN) == 0) {
            return i;
        }
    }
    return -1;
}

// Saves a node's latest report (from this board, the simulator or a radio link)
bool network_update_report(struct River_network *network, const char *id,
    struct Node_report report, unsigned long now_min) {
    int index = network_find_node(network, id);
    if (index < 0) {
        return false;
    }
    network->nodes[index].report = report;
    network->nodes[index].has_report = true;
    network->nodes[index].last_update_min = now_min;
    return true;
}

// true if the node has reported in the last NODE_STALE_MIN minutes
bool node_is_fresh(struct River_node *node, unsigned long now_min) {
    return node->has_report && now_min - node->last_update_min <= NODE_STALE_MIN;
}

// How far the water travels from one node down to another, in metres.
// Returns -1 if the second node isn't downstream of the first.
long network_river_distance(struct River_network *network, int from_index, int to_index) {
    struct River_node *from = &network->nodes[from_index];
    struct River_node *to = &network->nodes[to_index];
    long total = 0;
    int branch = from->branch;
    unsigned long distance = from->distance_m;

    // Follow the water down, branch by branch, until it reaches the other node's branch
    for (int steps = 0; steps <= MAX_BRANCHES; steps++) {
        if (branch == to->branch) {
            if (to->distance_m < distance) {
                return -1;
            }
            return total + (long)(to->distance_m - distance);
        }
        struct River_branch *b = &network->branches[branch];
        if (b->flows_into < 0) {
            return -1;
        }
        if (b->length_m > distance) {
            total += (long)(b->length_m - distance);
        }
        distance = b->joins_at_m;
        branch = b->flows_into;
    }
    return -1;
}

/////////////////////////
//////// POSITIONS //////
/////////////////////////

// How many different positions (branch + distance) there are
int network_position_count(struct River_network *network) {
    int positions = 0;
    for (int i = 0; i < network->num_nodes; i++) {
        if (i == 0 || !same_position(&network->nodes[i], &network->nodes[i - 1])) {
            positions++;
        }
    }
    return positions;
}

// Distance of a position (position 0 is the furthest upstream)
unsigned long network_position_distance(struct River_network *network, int position) {
    int seen = 0;
    for (int i = 0; i < network->num_nodes; i++) {
        if (i == 0 || !same_position(&network->nodes[i], &network->nodes[i - 1])) {
            if (seen == position) {
                return network->nodes[i].distance_m;
            }
            seen++;
        }
    }
    return 0;
}

// Branch of a position
int network_position_branch(struct River_network *network, int position) {
    int seen = 0;
    for (int i = 0; i < network->num_nodes; i++) {
        if (i == 0 || !same_position(&network->nodes[i], &network->nodes[i - 1])) {
            if (seen == position) {
                return network->nodes[i].branch;
            }
            seen++;
        }
    }
    return 0;
}

// Fills indexes[] with the nodes at that spot, returns how many
int network_nodes_at(struct River_network *network, int branch, unsigned long distance_m,
    int indexes[], int max) {
    int n = 0;
    for (int i = 0; i < network->num_nodes && n < max; i++) {
        if (network->nodes[i].branch == branch && network->nodes[i].distance_m == distance_m) {
            indexes[n] = i;
            n++;
        }
    }
    return n;
}

/////////////////////////
/////// ASSESSMENT //////
/////////////////////////

// Compares this node with the others to guess where a problem is coming from
struct Network_assessment network_assess(struct River_network *network, const char *local_id,
    unsigned long now_min) {
    struct Network_assessment a;
    memset(&a, 0, sizeof(struct Network_assessment)); // sets every value in the struct to 0
    a.finding = NF_NONE;
    a.upstream_gap_m = -1;
    a.branched = network->num_branches > 1;
    copy_string(a.local_id, local_id, NODE_ID_LEN);

    int local_index = network_find_node(network, local_id);
    if (local_index < 0 || !network->nodes[local_index].has_report) {
        return a;
    }
    struct River_node *local = &network->nodes[local_index];
    a.local_distance = local->distance_m;
    copy_string(a.local_branch, network->branches[local->branch].name, BRANCH_NAME_LEN);

    // Count the fresh nodes, the ones at the same position, and the ones seeing rain
    for (int i = 0; i < network->num_nodes; i++) {
        struct River_node *node = &network->nodes[i];
        if (i != local_index && node_is_fresh(node, now_min)) {
            a.peers++;
            if (node->report.rain_pattern || report_has_label(node->report, LBL_RAIN)) {
                a.rain_peers++;
            }
            if (same_position(node, local)) {
                a.siblings++;
                if (node->report.state == ST_ALERT) {
                    a.siblings_alerting++;
                }
            }
        }
    }

    // The nearest fresh node(s) upstream. Where two streams join above this
    // node, there's one for each stream.
    int upstream[MAX_NODES];
    int num_upstream = 0;
    find_upstream_nodes(network, local->branch, local->distance_m, now_min, upstream, &num_upstream, 0);

    // Is any of them seeing the same pollution? The finding is about that one
    // if so, otherwise about the last one in the list.
    bool upstream_same = false;
    int chosen = -1;
    for (int k = 0; k < num_upstream; k++) {
        struct Node_report *r = &network->nodes[upstream[k]].report;
        if (r->state == ST_ALERT && r->label == local->report.label) {
            if (!upstream_same || upstream[k] > chosen) {
                chosen = upstream[k];
            }
            upstream_same = true;
        } else if (!upstream_same && upstream[k] > chosen) {
            chosen = upstream[k];
        }
    }

    if (num_upstream > 0) {
        struct River_node *up = &network->nodes[chosen];
        a.has_upstream = true;
        copy_string(a.upstream_id, up->id, NODE_ID_LEN);
        a.upstream_distance = up->distance_m;
        copy_string(a.upstream_branch, network->branches[up->branch].name, BRANCH_NAME_LEN);
        a.upstream_gap_m = network_river_distance(network, chosen, local_index);
        a.num_upstream = num_upstream;
        for (int k = 0; k < num_upstream; k++) {
            if (k > 0) {
                strncat(a.upstream_list, ", ", UPSTREAM_LIST_LEN - strlen(a.upstream_list) - 1);
            }
            strncat(a.upstream_list, network->nodes[upstream[k]].id,
                    UPSTREAM_LIST_LEN - strlen(a.upstream_list) - 1);
        }
    }

    bool local_rain = local->report.rain_pattern || report_has_label(local->report, LBL_RAIN);
    if (local->report.state == ST_ALERT) {
        if (upstream_same) {
            a.finding = NF_FROM_UPSTREAM;
        } else if (a.siblings > 0 && a.siblings_alerting == a.siblings) {
            a.finding = NF_SOURCE_CROSS_SECTION;
        } else if (a.siblings > 0) {
            a.finding = NF_SOURCE_LOCAL_SIDE;
        } else if (a.has_upstream) {
            a.finding = NF_SOURCE_BETWEEN;
        } else if (a.peers > 0) {
            a.finding = NF_NO_UPSTREAM;
        } else {
            a.finding = NF_NO_PEERS;
        }
    } else if (local_rain) {
        // Rain hits every node at once, a leak shows up at one node first
        if (a.peers == 0) {
            a.finding = NF_NO_PEERS;
        } else if ((a.rain_peers + 1) * 2 > a.peers + 1) {
            a.finding = NF_RAIN_CONFIRMED;
        } else {
            a.finding = NF_RAIN_UNCONFIRMED;
        }
    }
    return a;
}

/////////////////////////
// Function definitions//
/////////////////////////

// Finds the nearest fresh nodes upstream of the spot (branch, below_m) and
// adds their indexes to found[]. On this branch that's the closest position
// above the spot. Any stream that joins in between is searched as well (from
// its bottom end), so each stream gives its own nearest node.
void find_upstream_nodes(struct River_network *network, int branch, unsigned long below_m,
    unsigned long now_min, int found[], int *num_found, int depth) {
    if (depth > MAX_BRANCHES) {
        return; // stops a badly set up network (branches in a loop) going forever
    }

    // Closest fresh position on this branch above the spot
    bool has_closest = false;
    unsigned long closest = 0;
    for (int i = 0; i < network->num_nodes; i++) {
        struct River_node *node = &network->nodes[i];
        if (node->branch == branch && node->distance_m < below_m && node_is_fresh(node, now_min)) {
            if (!has_closest || node->distance_m > closest) {
                closest = node->distance_m;
                has_closest = true;
            }
        }
    }

    // Streams that join this branch between that position and the spot
    for (int t = 0; t < network->num_branches; t++) {
        struct River_branch *stream = &network->branches[t];
        if (stream->flows_into == branch && stream->joins_at_m <= below_m &&
            (!has_closest || stream->joins_at_m > closest)) {
            find_upstream_nodes(network, t, FAR_DOWNSTREAM, now_min, found, num_found, depth + 1);
        }
    }

    if (has_closest) {
        for (int i = 0; i < network->num_nodes && *num_found < MAX_NODES; i++) {
            struct River_node *node = &network->nodes[i];
            if (node->branch == branch && node->distance_m == closest && node_is_fresh(node, now_min)) {
                found[*num_found] = i;
                *num_found = *num_found + 1;
            }
        }
    }
}

// Copies source into destination (size chars) and always ends it with '\0'.
// A NULL source gives an empty string.
void copy_string(char *destination, const char *source, int size) {
    if (source == NULL) {
        source = "";
    }
    strncpy(destination, source, size - 1);
    destination[size - 1] = '\0';
}

// Puts the node into the array, keeping it in upstream -> downstream order
bool insert_sorted(struct River_network *network, struct River_node node) {
    if (network->num_nodes >= MAX_NODES) {
        return false;
    }
    int i = 0;
    while (i < network->num_nodes && !comes_before(network, &node, &network->nodes[i])) {
        i++;
    }
    // Shift everything from i onwards one spot to the right to make room
    for (int j = network->num_nodes; j > i; j--) {
        network->nodes[j] = network->nodes[j - 1];
    }
    network->nodes[i] = node;
    network->num_nodes++;
    return true;
}

// true if node a goes before node b in the array: streams that are further
// from the end of the river go first, then by branch, then by distance
bool comes_before(struct River_network *network, struct River_node *a, struct River_node *b) {
    int depth_a = branch_depth(network, a->branch);
    int depth_b = branch_depth(network, b->branch);
    if (depth_a != depth_b) {
        return depth_a > depth_b;
    }
    if (a->branch != b->branch) {
        return a->branch < b->branch;
    }
    return a->distance_m < b->distance_m;
}

// How many joins the water goes through before it reaches a branch that
// doesn't flow into anything (0 for the main river)
int branch_depth(struct River_network *network, int branch) {
    int depth = 0;
    while (depth < MAX_BRANCHES && network->branches[branch].flows_into >= 0) {
        branch = network->branches[branch].flows_into;
        depth++;
    }
    return depth;
}

// true if two nodes are at the same spot (same branch and distance)
bool same_position(struct River_node *a, struct River_node *b) {
    return a->branch == b->branch && a->distance_m == b->distance_m;
}
