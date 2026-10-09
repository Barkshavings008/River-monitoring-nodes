#pragma once
// Turns codes in a report into words. Used for the serial monitor
// (display.cpp) and the phone messages (phone_messages.cpp), so both say the
// same thing. (Only snprintf in here, so it also builds for the PC tests)
#include "types.h"
#include "network.h"

////////////////////////
// Function prototypes//
////////////////////////
const char *fault_code_text(enum Fault_code fault);
const char *fault_reason(int sensor, enum Fault_code fault);
const char *finding_code(enum Net_finding finding);
void label_code(struct Node_report report, char *buffer, int size);
void finding_text(struct Network_assessment a, char *buffer, int size);
void node_status(struct River_network *network, struct River_node *node, unsigned long now_min,
    char *status, int size);
////////////////////////
