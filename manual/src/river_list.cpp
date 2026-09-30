#include "river_list.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

////////////////////////
// Function prototypes//
////////////////////////
struct River_node *insert_sorted(struct River_node *head, struct River_node *node);
struct River_node *unlink_node(struct River_node *head, struct River_node *node);
void copy_text(char *destination, const char *source, int size);
////////////////////////

/////////////////////////
/// ADDING / REMOVING ///
/////////////////////////

// Makes a new node and puts it in the right spot for its distance (after any
// nodes already at that distance). If the id is empty or already used, or
// there's no memory left, nothing is added and a warning is printed.
// Ids longer than 7 characters and places longer than 19 get cut short.
struct River_node *add_node(struct River_node *head, const char *id, float distance_m, const char *place) {
    if (id == NULL || id[0] == '\0') {
        Serial.println("add_node: the id can't be empty");
        return head;
    }
    if (find_node(head, id) != NULL) {
        Serial.print("add_node: there's already a node called ");
        Serial.println(id);
        return head;
    }

    struct River_node *node = (struct River_node *)malloc(sizeof(struct River_node));
    if (node == NULL) {
        Serial.println("add_node: out of memory");
        return head;
    }
    copy_text(node->id, id, NODE_ID_LEN);
    copy_text(node->place, place, NODE_PLACE_LEN);
    node->distance_m = distance_m;
    node->ph = 0.0f;
    node->tds = 0.0f;
    node->turbidity = 0.0f;
    node->water_temp = 0.0f;
    node->has_reading = false;
    node->last_update_ms = 0;
    node->next = NULL;

    return insert_sorted(head, node);
}

// Removes the node with this id and frees its memory.
// If there's no node with that id, nothing happens.
struct River_node *delete_node(struct River_node *head, const char *id) {
    struct River_node *node = find_node(head, id);
    if (node == NULL) {
        return head;
    }
    head = unlink_node(head, node);
    free(node);
    return head;
}

// Moves a node to a new distance, keeping its readings.
// If there's no node with that id, nothing happens.
struct River_node *move_node(struct River_node *head, const char *id, float new_distance_m) {
    struct River_node *node = find_node(head, id);
    if (node == NULL) {
        return head;
    }
    head = unlink_node(head, node);
    node->distance_m = new_distance_m;
    return insert_sorted(head, node);
}

// Frees every node. Always returns NULL, so use it like: head = delete_all_nodes(head);
struct River_node *delete_all_nodes(struct River_node *head) {
    struct River_node *current = head;
    while (current != NULL) {
        struct River_node *next = current->next; // save it before freeing current
        free(current);
        current = next;
    }
    return NULL;
}

/////////////////////////
//////// FINDING ////////
/////////////////////////

struct River_node *find_node(struct River_node *head, const char *id) {
    if (id == NULL) {
        return NULL;
    }
    struct River_node *current = head;
    while (current != NULL) {
        if (strcmp(current->id, id) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// The closest node further upstream (smaller distance). Nodes at the same
// distance as this one don't count. NULL if this is the furthest upstream.
struct River_node *find_upstream_node(struct River_node *head, struct River_node *node) {
    if (node == NULL) {
        return NULL;
    }
    struct River_node *closest = NULL;
    struct River_node *current = head;
    while (current != NULL && current->distance_m < node->distance_m) {
        closest = current; // the list is in order, so the last one found is the closest
        current = current->next;
    }
    return closest;
}

// The closest node further downstream (bigger distance). Nodes at the same
// distance as this one don't count. NULL if this is the furthest downstream.
struct River_node *find_downstream_node(struct River_node *node) {
    if (node == NULL) {
        return NULL;
    }
    struct River_node *current = node->next;
    while (current != NULL && current->distance_m <= node->distance_m) {
        current = current->next;
    }
    return current;
}

int count_nodes(struct River_node *head) {
    int count = 0;
    struct River_node *current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

float distance_between(struct River_node *head, const char *id_a, const char *id_b) {
    struct River_node *a = find_node(head, id_a);
    struct River_node *b = find_node(head, id_b);
    if (a == NULL || b == NULL) {
        return -1.0f;
    }
    return fabsf(a->distance_m - b->distance_m);
}

/////////////////////////
//////// READINGS ///////
/////////////////////////

// Saves new readings into a node (e.g. ones sent from another board).
// Returns false if node is NULL.
bool update_readings(struct River_node *node, float ph, float tds, float turbidity, float water_temp) {
    if (node == NULL) {
        return false;
    }
    node->ph = ph;
    node->tds = tds;
    node->turbidity = turbidity;
    node->water_temp = water_temp;
    node->has_reading = true;
    node->last_update_ms = millis();
    return true;
}

// Reads this board's sensors and saves the readings into the node
// (use it on the node that is this board). Returns false if node is NULL.
bool update_from_sensors(struct River_node *node) {
    if (node == NULL) {
        return false;
    }
    struct Water_reading reading = read_all_sensors();
    return update_readings(node, reading.ph, reading.tds, reading.turbidity, reading.water_temp);
}

// How long ago the readings were updated, in ms. Gives 0xFFFFFFFF if the
// node has never had a reading (or node is NULL).
unsigned long reading_age_ms(struct River_node *node) {
    if (node == NULL || !node->has_reading) {
        return 0xFFFFFFFF;
    }
    return millis() - node->last_update_ms;
}

/////////////////////////
//////// PRINTING ///////
/////////////////////////

// N1 (left bank) at 1000 m
//   pH 7.21 | TDS 210 mg/L | Turbidity 8.1 NTU | Water temp 17.1 C | 2 s ago
void print_node(struct River_node *node) {
    if (node == NULL) {
        Serial.println("(no node)");
        return;
    }
    Serial.print(node->id);
    Serial.print(" (");
    Serial.print(node->place);
    Serial.print(") at ");
    Serial.print(node->distance_m, 0);
    Serial.println(" m");

    if (!node->has_reading) {
        Serial.println("  no readings yet");
        return;
    }
    Serial.print("  pH ");
    Serial.print(node->ph, 2);
    Serial.print(" | TDS ");
    Serial.print(node->tds, 0);
    Serial.print(" mg/L | Turbidity ");
    Serial.print(node->turbidity, 1);
    Serial.print(" NTU | Water temp ");
    Serial.print(node->water_temp, 1);
    Serial.print(" C | ");
    Serial.print(reading_age_ms(node) / 1000);
    Serial.println(" s ago");
}

// Every node from upstream to downstream
void print_all_nodes(struct River_node *head) {
    Serial.print("River nodes, upstream -> downstream (");
    Serial.print(count_nodes(head));
    Serial.println(" total):");
    if (head == NULL) {
        Serial.println("  (list is empty)");
    }
    struct River_node *current = head;
    while (current != NULL) {
        print_node(current);
        current = current->next;
    }
    Serial.println("-----------------------------");
}

/////////////////////////
// Function definitions//
/////////////////////////

// Puts an already made node into the list in distance order
struct River_node *insert_sorted(struct River_node *head, struct River_node *node) {
    // Goes at the very start (empty list, or further upstream than everything)
    if (head == NULL || node->distance_m < head->distance_m) {
        node->next = head;
        return node;
    }
    // Otherwise walk along until the next node is further downstream
    struct River_node *current = head;
    while (current->next != NULL && current->next->distance_m <= node->distance_m) {
        current = current->next;
    }
    node->next = current->next;
    current->next = node;
    return head;
}

// Takes a node out of the list without freeing it
struct River_node *unlink_node(struct River_node *head, struct River_node *node) {
    if (head == node) {
        return head->next;
    }
    struct River_node *current = head;
    while (current != NULL && current->next != node) {
        current = current->next;
    }
    if (current != NULL) {
        current->next = node->next;
    }
    node->next = NULL;
    return head;
}

// Copies source into destination (size chars) and always ends it with '\0'
void copy_text(char *destination, const char *source, int size) {
    if (source == NULL) {
        source = "";
    }
    strncpy(destination, source, size - 1);
    destination[size - 1] = '\0';
}
