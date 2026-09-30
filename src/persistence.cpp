#include "persistence.h"
#include <string.h>

void persistence_reset(struct Persistence *persistence) {
    memset(persistence, 0, sizeof(struct Persistence)); // sets every value in the struct to 0
}

// hits = what the rules found this minute. Any label that turns on or off
// gets written into events. Returns how many events there were.
int persistence_update(struct Persistence *persistence, struct Rule_result hits[], int num_hits,
    struct Alert_event events[], int max_events) {
    int num_events = 0;

    for (int id = LBL_NONE + 1; id < LBL_FAULT; id++) {
        // This label's confidence this minute (-1 if the rule didn't fire)
        float confidence = -1.0f;
        for (int i = 0; i < num_hits; i++) {
            if (hits[i].id == id) {
                confidence = hits[i].confidence;
            }
        }

        bool changed = false;
        if (confidence >= 0.0f) {
            persistence->off_streak[id] = 0;
            if (persistence->on_streak[id] < 255) {
                persistence->on_streak[id]++;
            }
            persistence->confidence[id] = confidence;
            if (!persistence->active[id] && persistence->on_streak[id] >= PERSIST_ON_MIN) {
                persistence->active[id] = true;
                changed = true;
            }
        } else {
            persistence->on_streak[id] = 0;
            if (persistence->off_streak[id] < 255) {
                persistence->off_streak[id]++;
            }
            if (persistence->active[id] && persistence->off_streak[id] >= PERSIST_OFF_MIN) {
                persistence->active[id] = false;
                changed = true;
            }
        }

        if (changed && num_events < max_events) {
            events[num_events].id = (enum Label_id)id;
            events[num_events].on = persistence->active[id];
            events[num_events].confidence = persistence->confidence[id];
            num_events++;
        }
    }
    return num_events;
}

// true if any label is on
bool persistence_any_active(struct Persistence *persistence) {
    for (int id = LBL_NONE + 1; id < LBL_FAULT; id++) {
        if (persistence->active[id]) {
            return true;
        }
    }
    return false;
}
