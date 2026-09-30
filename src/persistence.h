#pragma once
// Section 5: a label turns on after PERSIST_ON_MIN minutes in a row of seeing
// it, and off after PERSIST_OFF_MIN minutes in a row of not seeing it.
#include "types.h"

// A label turning on or off
struct Alert_event {
    enum Label_id id;
    bool on;
    float confidence;
};

struct Persistence {
    int on_streak[LBL_COUNT];
    int off_streak[LBL_COUNT];
    bool active[LBL_COUNT];
    float confidence[LBL_COUNT];
};

////////////////////////
// Function prototypes//
////////////////////////
void persistence_reset(struct Persistence *persistence);
int persistence_update(struct Persistence *persistence, struct Rule_result hits[], int num_hits,
    struct Alert_event events[], int max_events);
bool persistence_any_active(struct Persistence *persistence);
////////////////////////
