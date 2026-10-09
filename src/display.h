#pragma once
// Everything that gets printed to the serial monitor is in here. The rules
// and the engine never print anything themselves.
#include "types.h"
#include "network.h"
#include "persistence.h"
#include "sensors.h"
#include "water_speed.h"

////////////////////////
// Function prototypes//
////////////////////////
void print_banner(struct River_network *network);
void print_help(void);
void print_alert_events(struct Alert_event events[], int num_events);
void print_speed_estimates(struct Speed_estimate speeds[], int num_speeds);
void print_minute_report(struct Node_report report, struct Network_assessment assessment,
    struct River_network *network, unsigned long now_min);
void print_network_list(struct River_network *network, unsigned long now_min);
void print_message(const char *message);
void print_voltages(struct Sensor_voltages volts);
////////////////////////
