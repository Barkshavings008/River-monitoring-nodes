#include "water_speed.h"
#include "labels.h"
#include <string.h>

////////////////////////
// Function prototypes//
////////////////////////
struct Node_watch *watch_for(struct Speed_tracker *tracker, struct River_network *network,
    const char *id);
unsigned long alert_mask(struct Node_report report);
bool pair_with_upstream(struct Speed_tracker *tracker, struct River_network *network,
    int to_index, enum Label_id label, unsigned long now_min, struct Speed_estimate *estimate);
void remember_detection(struct Speed_tracker *tracker, const char *id, enum Label_id label,
    unsigned long now_min);
////////////////////////

void speed_tracker_begin(struct Speed_tracker *tracker) {
    memset(tracker, 0, sizeof(struct Speed_tracker));
}

// Looks at every node's newest report. Each pollution/watch label that just
// turned on after a quiet spell is paired with the same label arriving
// further upstream earlier. Any new speeds go in found[] (up to max_found),
// and the number found is returned. The newest one is also kept in
// tracker->latest.
int speed_tracker_update(struct Speed_tracker *tracker, struct River_network *network,
    unsigned long now_min, struct Speed_estimate found[], int max_found) {
    int num_found = 0;

    // Rain anywhere stops pairs being made across it
    for (int i = 0; i < network->num_nodes; i++) {
        struct River_node *node = &network->nodes[i];
        if (node_is_fresh(node, now_min) && report_has_label(node->report, LBL_RAIN)) {
            tracker->rain_seen = true;
            tracker->last_rain_min = now_min;
        }
    }

    for (int i = 0; i < network->num_nodes; i++) {
        struct River_node *node = &network->nodes[i];
        if (!node->has_report) {
            continue;
        }
        struct Node_watch *watch = watch_for(tracker, network, node->id);
        if (watch == NULL || (watch->looked && watch->last_report_min == node->last_update_min)) {
            continue; // no room, or this report has already been looked at
        }
        bool first_look = !watch->looked;
        watch->looked = true;
        watch->last_report_min = node->last_update_min;

        // Labels already on the first time a node is seen don't count as
        // arriving (they could have been on for hours)
        unsigned long mask = alert_mask(node->report);
        unsigned long turned_on = mask & ~watch->active_mask;
        bool fresh = !watch->ever_active ||
            node->last_update_min - watch->last_active_min > SPEED_QUIET_MIN;
        if (first_look) {
            fresh = false;
        }

        if (fresh && turned_on != 0) {
            for (int id = 0; id < LBL_COUNT; id++) {
                if ((turned_on & (1UL << id)) == 0) {
                    continue;
                }
                struct Speed_estimate estimate;
                if (pair_with_upstream(tracker, network, i, (enum Label_id)id,
                        node->last_update_min, &estimate)) {
                    tracker->latest = estimate;
                    tracker->has_estimate = true;
                    if (num_found < max_found) {
                        found[num_found] = estimate;
                        num_found++;
                    }
                }
                remember_detection(tracker, node->id, (enum Label_id)id, node->last_update_min);
            }
        }

        watch->active_mask = mask;
        if (mask != 0) {
            watch->ever_active = true;
            watch->last_active_min = node->last_update_min;
        }
    }
    return num_found;
}

// true for the labels that mean pollution or something to watch
bool is_alert_label(enum Label_id id) {
    enum Category category = get_label_info(id).category;
    return category == CAT_POLLUTION || category == CAT_WATCH;
}

/////////////////////////
// Function definitions//
/////////////////////////

// The node's slot in the tracker. A new slot is made the first time a node
// is seen, and slots of nodes that were deleted get reused.
struct Node_watch *watch_for(struct Speed_tracker *tracker, struct River_network *network,
    const char *id) {
    for (int w = 0; w < MAX_NODES; w++) {
        if (tracker->watch[w].used && strncmp(tracker->watch[w].id, id, NODE_ID_LEN) == 0) {
            return &tracker->watch[w];
        }
    }
    for (int w = 0; w < MAX_NODES; w++) {
        struct Node_watch *watch = &tracker->watch[w];
        if (!watch->used || network_find_node(network, watch->id) < 0) {
            memset(watch, 0, sizeof(struct Node_watch));
            strncpy(watch->id, id, NODE_ID_LEN - 1);
            watch->id[NODE_ID_LEN - 1] = '\0';
            watch->used = true;
            return watch;
        }
    }
    return NULL;
}

// One bit for each pollution/watch label that is on in the report
unsigned long alert_mask(struct Node_report report) {
    unsigned long mask = 0;
    if (is_alert_label(report.label)) {
        mask |= 1UL << report.label;
    }
    for (int i = 0; i < report.num_also; i++) {
        if (is_alert_label(report.also[i])) {
            mask |= 1UL << report.also[i];
        }
    }
    return mask;
}

// Finds the closest node upstream that had the same label arrive in the last
// SPEED_PAIR_WINDOW_MIN minutes (with no rain since), and works out the speed.
// If two arrivals are the same distance away, the newest one is used.
bool pair_with_upstream(struct Speed_tracker *tracker, struct River_network *network,
    int to_index, enum Label_id label, unsigned long now_min, struct Speed_estimate *estimate) {
    int best = -1;
    long best_metres = 0;
    for (int d = 0; d < tracker->num_detections; d++) {
        struct Plume_detection *det = &tracker->detections[d];
        if (det->label != label || det->minute >= now_min ||
            now_min - det->minute > SPEED_PAIR_WINDOW_MIN) {
            continue;
        }
        if (tracker->rain_seen && tracker->last_rain_min >= det->minute) {
            continue;
        }
        int from_index = network_find_node(network, det->node_id);
        if (from_index < 0 || from_index == to_index) {
            continue;
        }
        long metres = network_river_distance(network, from_index, to_index);
        if (metres <= 0) {
            continue; // not upstream of this node (or at the same spot)
        }
        bool closer = best < 0 || metres < best_metres;
        bool same_but_newer = metres == best_metres && det->minute > tracker->detections[best].minute;
        if (closer || same_but_newer) {
            best = d;
            best_metres = metres;
        }
    }
    if (best < 0) {
        return false;
    }

    unsigned long minutes = now_min - tracker->detections[best].minute;
    float speed = (float)best_metres / (float)(minutes * 60);
    if (speed > SPEED_MAX_MPS) {
        return false;
    }
    strncpy(estimate->from_id, tracker->detections[best].node_id, NODE_ID_LEN);
    strncpy(estimate->to_id, network->nodes[to_index].id, NODE_ID_LEN);
    estimate->label = label;
    estimate->metres = (unsigned long)best_metres;
    estimate->minutes = minutes;
    estimate->metres_per_sec = speed;
    estimate->found_min = now_min;
    return true;
}

// Saves an arrival. When the list is full the oldest one is dropped.
void remember_detection(struct Speed_tracker *tracker, const char *id, enum Label_id label,
    unsigned long now_min) {
    if (tracker->num_detections >= MAX_DETECTIONS) {
        for (int d = 1; d < MAX_DETECTIONS; d++) {
            tracker->detections[d - 1] = tracker->detections[d];
        }
        tracker->num_detections = MAX_DETECTIONS - 1;
    }
    struct Plume_detection *det = &tracker->detections[tracker->num_detections];
    strncpy(det->node_id, id, NODE_ID_LEN - 1);
    det->node_id[NODE_ID_LEN - 1] = '\0';
    det->label = label;
    det->minute = now_min;
    tracker->num_detections++;
}
