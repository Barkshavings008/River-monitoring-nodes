#include <Arduino.h>
#include "config.h"
#include "sensors.h"
#include "river_list.h"

// Short example of the helper functions. Change or delete any of it and
// write your own logic.

#define THIS_NODE "N1"

struct River_node *head = NULL; // start of the linked list of river nodes

void setup() {
    Serial.begin(115200);
    delay(200);
    wake_up_sensors();

    // Nodes can be added in any order, the list sorts itself by distance
    head = add_node(head, "N0", 0, "upstream");
    head = add_node(head, THIS_NODE, 1000, "left bank");
    head = add_node(head, "N2", 1000, "right bank");
    head = add_node(head, "N3", 2500, "downstream");

    print_all_nodes(head);
}

void loop() {
    // One reading at a time...
    float ph = return_ph();
    Serial.print("pH: ");
    Serial.println(ph);

    // ...or everything at once, saved straight into this board's node
    struct River_node *me = find_node(head, THIS_NODE);
    update_from_sensors(me);
    print_node(me);

    // Getting around the list
    struct River_node *upstream = find_upstream_node(head, me);
    if (upstream != NULL) {
        Serial.print("Nearest upstream node is ");
        Serial.print(upstream->id);
        Serial.print(", ");
        Serial.print(distance_between(head, me->id, upstream->id), 0);
        Serial.println(" m away");
    }

    Serial.println("-----------------------------");
    delay(2000);
}
