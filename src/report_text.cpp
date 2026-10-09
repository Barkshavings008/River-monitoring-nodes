#include "report_text.h"
#include "labels.h"
#include <stdio.h>

const char *fault_code_text(enum Fault_code fault) {
    if (fault == F_NO_READING) {
        return "NO_READING";
    } else if (fault == F_OUT_OF_RANGE) {
        return "OUT_OF_RANGE";
    } else if (fault == F_FLATLINE) {
        return "FLATLINE";
    } else if (fault == F_TEMP_JUMP) {
        return "JUMP";
    } else {
        return "NONE";
    }
}

// What the fault means in plain words
const char *fault_reason(int sensor, enum Fault_code fault) {
    if (fault == F_NO_READING) {
        if (sensor == S_TEMP) {
            return "no reading (DS18B20 gave -127/85 C); using 25 C for correction";
        } else {
            return "no reading";
        }
    } else if (fault == F_OUT_OF_RANGE) {
        return "reading out of range - check probe, wiring or calibration";
    } else if (fault == F_FLATLINE) {
        return "reading stuck for 60+ min - check probe and wiring";
    } else if (fault == F_TEMP_JUMP) {
        return "jumped more than 3 C and straight back - check probe";
    } else {
        return "";
    }
}

const char *finding_code(enum Net_finding finding) {
    if (finding == NF_NO_PEERS) {
        return "NO_PEERS";
    } else if (finding == NF_SOURCE_LOCAL_SIDE) {
        return "SOURCE_LOCAL_SIDE";
    } else if (finding == NF_SOURCE_CROSS_SECTION) {
        return "SOURCE_CROSS_SECTION";
    } else if (finding == NF_SOURCE_BETWEEN) {
        return "SOURCE_BETWEEN";
    } else if (finding == NF_FROM_UPSTREAM) {
        return "FROM_UPSTREAM";
    } else if (finding == NF_RAIN_CONFIRMED) {
        return "RAIN_CONFIRMED";
    } else if (finding == NF_RAIN_UNCONFIRMED) {
        return "RAIN_UNCONFIRMED";
    } else if (finding == NF_NO_UPSTREAM) {
        return "NO_UPSTREAM";
    } else {
        return "NONE";
    }
}

// Writes the report's label code into buffer. For a fault it names the first
// broken sensor, e.g. FAULT_TEMP_NO_READING
void label_code(struct Node_report report, char *buffer, int size) {
    if (report.label != LBL_FAULT) {
        snprintf(buffer, size, "%s", get_label_info(report.label).code);
        return;
    }
    for (int s = 0; s < S_COUNT; s++) {
        if (report.fault[s] != F_NONE) {
            snprintf(buffer, size, "FAULT_%s_%s", sensor_code(s), fault_code_text(report.fault[s]));
            return;
        }
    }
    if (report.ph_recalibrate) {
        snprintf(buffer, size, "PH_RECALIBRATE");
    } else {
        snprintf(buffer, size, "FAULT");
    }
}

// Writes a sentence about what the network comparison found into buffer
// (empty if there's nothing to say). Places look like "N0 (1000 m)", or
// "N0 (stream_a 800 m)" when the network has more than one branch.
void finding_text(struct Network_assessment a, char *buffer, int size) {
    char here[32];      // where this node is
    char local[48];     // this node and where it is
    char upstream[48];  // the upstream node and where it is
    if (a.branched) {
        snprintf(here, 32, "%s %u m", a.local_branch, (unsigned)a.local_distance);
        snprintf(upstream, 48, "%s (%s %u m)", a.upstream_id, a.upstream_branch,
                 (unsigned)a.upstream_distance);
    } else {
        snprintf(here, 32, "%u m", (unsigned)a.local_distance);
        snprintf(upstream, 48, "%s (%u m)", a.upstream_id, (unsigned)a.upstream_distance);
    }
    snprintf(local, 48, "%s (%s)", a.local_id, here);
    unsigned rain_nodes = (unsigned)a.rain_peers + 1;
    unsigned all_nodes = (unsigned)a.peers + 1;

    if (a.finding == NF_NO_PEERS) {
        snprintf(buffer, size, "No fresh data from other nodes - cannot compare.");
    } else if (a.finding == NF_SOURCE_LOCAL_SIDE) {
        if (a.has_upstream) {
            snprintf(buffer, size, "Source likely between %s and %s, on %s's side - "
                     "other node(s) at this position are normal.", upstream, local, a.local_id);
        } else {
            snprintf(buffer, size, "Source likely on %s's side at %s - other node(s) here are normal.",
                     a.local_id, here);
        }
    } else if (a.finding == NF_SOURCE_CROSS_SECTION) {
        if (a.has_upstream) {
            snprintf(buffer, size, "Whole river width affected at %s; source likely between %s "
                     "and here.", here, upstream);
        } else {
            snprintf(buffer, size, "Whole river width affected at %s.", here);
        }
    } else if (a.finding == NF_SOURCE_BETWEEN) {
        if (a.num_upstream > 1) {
            snprintf(buffer, size, "Source likely between %s and the nearest upstream nodes (%s) - "
                     "all of them are normal.", local, a.upstream_list);
        } else {
            snprintf(buffer, size, "Source likely between %s and %s - upstream is normal.",
                     upstream, local);
        }
    } else if (a.finding == NF_FROM_UPSTREAM) {
        if (a.branched && a.upstream_gap_m >= 0) {
            snprintf(buffer, size, "Same pattern upstream at %s, %ld m up the river - pollution is "
                     "coming from further upstream.", upstream, a.upstream_gap_m);
        } else {
            snprintf(buffer, size, "Same pattern upstream at %s - pollution is coming from further "
                     "upstream.", upstream);
        }
    } else if (a.finding == NF_RAIN_CONFIRMED) {
        snprintf(buffer, size, "Rain pattern at %u of %u nodes - confirmed weather event.",
                 rain_nodes, all_nodes);
    } else if (a.finding == NF_NO_UPSTREAM) {
        snprintf(buffer, size, "No nodes upstream of %s to compare with - the source is somewhere "
                 "above it.", a.local_id);
    } else if (a.finding == NF_RAIN_UNCONFIRMED) {
        snprintf(buffer, size, "Rain pattern at this node only (%u of %u) - possible stormwater outfall "
                 "or leak nearby.", rain_nodes, all_nodes);
    } else {
        buffer[0] = '\0';
    }
}

// Writes one node's status for the network table into status
void node_status(struct River_network *network, struct River_node *node, unsigned long now_min,
    char *status, int size) {
    if (!node->has_report) {
        snprintf(status, size, "no data");
    } else if (!node->is_local && !node_is_fresh(node, now_min)) {
        snprintf(status, size, "offline");
    } else if (node->report.label == LBL_NONE) {
        snprintf(status, size, "%s", state_name(node->report.state));
    } else {
        char code[40];
        label_code(node->report, code, 40);
        snprintf(status, size, "%s %s", state_name(node->report.state), code);
    }
}
