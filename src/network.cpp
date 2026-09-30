#include "network.h"
#include <string.h>

////////////////////////
// Function prototypes//
////////////////////////
void copy_string(char *destination, const char *source, int size);
bool insert_sorted(struct River_network *network, struct River_node node);
////////////////////////

void network_clear(struct River_network *network) {
    network->num_nodes = 0;
}

// Adds a node in distance order (after any nodes already at that distance).
// Fails if the id is empty or already used, or the network is full.
bool network_add_node(struct River_network *network, const char *id, unsigned long distance_m,
    const char *place, bool is_local) {
    if (id == NULL || id[0] == '\0') {
        return false;
    }
    if (network_find_node(network, id) >= 0 || network->num_nodes >= MAX_NODES) {
        return false;
    }
    struct River_node node;
    memset(&node, 0, sizeof(struct River_node)); // sets every value in the struct to 0
    copy_string(node.id, id, NODE_ID_LEN);
    copy_string(node.place, place, NODE_PLACE_LEN);
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

// Moves a node to a new distance, keeping its latest report
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

// How many different distances there are
int network_position_count(struct River_network *network) {
    int positions = 0;
    for (int i = 0; i < network->num_nodes; i++) {
        if (i == 0 || network->nodes[i].distance_m != network->nodes[i - 1].distance_m) {
            positions++;
        }
    }
    return positions;
}

// Distance of a position (position 0 is the furthest upstream)
unsigned long network_position_distance(struct River_network *network, int position) {
    int seen = 0;
    for (int i = 0; i < network->num_nodes; i++) {
        if (i == 0 || network->nodes[i].distance_m != network->nodes[i - 1].distance_m) {
            if (seen == position) {
                return network->nodes[i].distance_m;
            }
            seen++;
        }
    }
    return 0;
}

// Fills indexes[] with the nodes at that distance, returns how many
int network_nodes_at(struct River_network *network, unsigned long distance_m, int indexes[], int max) {
    int n = 0;
    for (int i = 0; i < network->num_nodes && n < max; i++) {
        if (network->nodes[i].distance_m == distance_m) {
            indexes[n] = i;
            n++;
        }
    }
    return n;
}

// Compares this node with the others to guess where a problem is coming from
struct Network_assessment network_assess(struct River_network *network, const char *local_id,
    unsigned long now_min) {
    struct Network_assessment a;
    memset(&a, 0, sizeof(struct Network_assessment)); // sets every value in the struct to 0
    a.finding = NF_NONE;
    copy_string(a.local_id, local_id, NODE_ID_LEN);

    int local_index = network_find_node(network, local_id);
    if (local_index < 0 || !network->nodes[local_index].has_report) {
        return a;
    }
    struct River_node *local = &network->nodes[local_index];
    a.local_distance = local->distance_m;

    // Count the fresh nodes, the ones at the same position, and the ones seeing rain
    for (int i = 0; i < network->num_nodes; i++) {
        struct River_node *node = &network->nodes[i];
        if (i != local_index && node_is_fresh(node, now_min)) {
            a.peers++;
            if (node->report.rain_pattern || report_has_label(node->report, LBL_RAIN)) {
                a.rain_peers++;
            }
            if (node->distance_m == local->distance_m) {
                a.siblings++;
                if (node->report.state == ST_ALERT) {
                    a.siblings_alerting++;
                }
            }
        }
    }

    // Find the nearest upstream position with fresh data. The array is in
    // distance order, so just walk backwards from this node.
    bool upstream_same = false;
    bool stop = false;
    for (int i = local_index - 1; i >= 0 && !stop; i--) {
        struct River_node *node = &network->nodes[i];
        if (node->distance_m != local->distance_m && node_is_fresh(node, now_min)) {
            if (!a.has_upstream) {
                a.has_upstream = true;
                a.upstream_distance = node->distance_m;
                copy_string(a.upstream_id, node->id, NODE_ID_LEN);
            }

            if (node->distance_m != a.upstream_distance) {
                stop = true; // gone past the nearest upstream position
            } else if (node->report.state == ST_ALERT && node->report.label == local->report.label) {
                upstream_same = true;
            }
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

// Copies source into destination (size chars) and always ends it with '\0'.
// A NULL source gives an empty string.
void copy_string(char *destination, const char *source, int size) {
    if (source == NULL) {
        source = "";
    }
    strncpy(destination, source, size - 1);
    destination[size - 1] = '\0';
}

// Puts the node into the array, keeping it in distance order
bool insert_sorted(struct River_network *network, struct River_node node) {
    if (network->num_nodes >= MAX_NODES) {
        return false;
    }
    int i = 0;
    while (i < network->num_nodes && network->nodes[i].distance_m <= node.distance_m) {
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
