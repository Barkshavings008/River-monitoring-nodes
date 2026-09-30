#pragma once
// Linked list of the monitoring stations (nodes) along the river.
//
// The list is always kept in order of distance down the river, so the first
// node is the furthest upstream and node->next is the next one downstream.
// Two nodes can have the same distance (e.g. the left and right bank at one spot).
//
//   head -> [N0, 0 m] -> [N1, 1000 m] -> [N2, 1000 m] -> [N3, 2500 m] -> NULL
//           upstream                                      downstream
//
// Functions that can change the first node return the new head, so always use
// them like:   head = add_node(head, ...);
#include "config.h"
#include "sensors.h"

struct River_node {
    char id[NODE_ID_LEN];         // e.g. "N1"
    char place[NODE_PLACE_LEN];   // e.g. "left bank"
    float distance_m;             // distance down the river from the start point

    // Latest readings (only real once has_reading is true)
    float ph;
    float tds;                    // mg/L
    float turbidity;              // NTU
    float water_temp;             // C
    bool has_reading;
    unsigned long last_update_ms; // millis() when the readings were last updated

    struct River_node *next;      // next node downstream (NULL = last one)
};

////////////////////////
// Function prototypes//
////////////////////////

// Adding and removing (these return the new head)
struct River_node *add_node(struct River_node *head, const char *id, float distance_m, const char *place);
struct River_node *delete_node(struct River_node *head, const char *id);
struct River_node *move_node(struct River_node *head, const char *id, float new_distance_m);
struct River_node *delete_all_nodes(struct River_node *head);

// Finding nodes (these return NULL if there isn't one)
struct River_node *find_node(struct River_node *head, const char *id);
struct River_node *find_upstream_node(struct River_node *head, struct River_node *node);
struct River_node *find_downstream_node(struct River_node *node);
int count_nodes(struct River_node *head);

// Readings
bool update_readings(struct River_node *node, float ph, float tds, float turbidity, float water_temp);
bool update_from_sensors(struct River_node *node);
unsigned long reading_age_ms(struct River_node *node);

// Distance between two nodes in metres (-1 if either id isn't in the list)
float distance_between(struct River_node *head, const char *id_a, const char *id_b);

// Printing
void print_node(struct River_node *node);
void print_all_nodes(struct River_node *head);
////////////////////////
