#pragma once
// All Serial output lives here. Rules and engines never print.
#include "types.h"
#include "network.h"
#include "persistence.h"
#include "sensors.h"

// Start-up banner: mode, timing and the network list.
void displayBanner(const RiverNetwork &net);

// One-line list of the serial commands.
void displayHelp();

// One-line ALERT ON/OFF messages, printed as soon as a label changes.
void displayEvents(const AlertEvent *events, uint8_t n);

// Per-minute human block and/or JSON line (OUTPUT_MODE).
void displayMinute(const NodeReport &r, const NetworkAssessment &a,
                   const RiverNetwork &net, uint32_t nowMin);

// Every position in the network and the nodes at it.
void displayNetworkList(const RiverNetwork &net, uint32_t nowMin);

void displayMessage(const char *msg);

// Raw sensor voltages, for calibration.
void displayVoltages(const SensorVoltages &v);
