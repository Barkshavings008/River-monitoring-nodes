#pragma once
// Works out how fast the water is moving from pollution travelling down the
// river. When the same kind of pollution arrives at an upstream node and then
// at a node further down:
//
//   N0 alert at 10:00  ---- 1500 m of river ---->  N1 alert at 10:25
//   speed = 1500 m / (25 * 60 s) = 1.0 m/s
//
// Only a fresh arrival counts (the node had no alerts for SPEED_QUIET_MIN
// before), so a plume that has been sitting there for hours isn't paired up.
// Rain pauses the pollution rules and the alerts restart afterwards, so no
// pair is made across rain.
#include "types.h"
#include "network.h"

// A fresh pollution arrival at one node
struct Plume_detection {
    char node_id[NODE_ID_LEN];
    enum Label_id label;
    unsigned long minute;
};

// What a node was doing last time its report was looked at
struct Node_watch {
    char id[NODE_ID_LEN];
    bool used;
    bool looked;                    // false until its first report has been looked at
    unsigned long last_report_min;  // so each report is only looked at once
    unsigned long active_mask;      // which pollution/watch labels were on (bit = Label_id)
    bool ever_active;
    unsigned long last_active_min;  // last minute any pollution/watch label was on
};

struct Speed_estimate {
    char from_id[NODE_ID_LEN];
    char to_id[NODE_ID_LEN];
    enum Label_id label;
    unsigned long metres;
    unsigned long minutes;
    float metres_per_sec;
    unsigned long found_min;        // when it was worked out
};

struct Speed_tracker {
    struct Node_watch watch[MAX_NODES];
    struct Plume_detection detections[MAX_DETECTIONS];
    int num_detections;
    bool rain_seen;
    unsigned long last_rain_min;
    bool has_estimate;
    struct Speed_estimate latest;
};

////////////////////////
// Function prototypes//
////////////////////////
void speed_tracker_begin(struct Speed_tracker *tracker);
int speed_tracker_update(struct Speed_tracker *tracker, struct River_network *network,
    unsigned long now_min, struct Speed_estimate found[], int max_found);
bool is_alert_label(enum Label_id id);
////////////////////////
