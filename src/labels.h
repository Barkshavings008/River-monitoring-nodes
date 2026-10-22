#pragma once
// Text that goes with each label. The rules only return the Label_id, the
// printing code uses get_label_info() to get the words.
#include "types.h"

struct Label_info {
    const char *code;         // machine name, e.g. LIKELY_HEAVY_METALS
    const char *name;         // long human name
    const char *short_name;   // for the ALERT ON/OFF lines
    enum Category category;       // what it's shown as (watch labels show as pollution when
                                  // DEMO_ALERTS is on)
    enum Category base_category;  // what it really is (used where that matters)
    const char *pollutants;
    const char *meaning;
};

#define DISCLAIMER_LINE1 "Pollutants listed are likely candidates only."
#define DISCLAIMER_LINE2 "Confirm with a lab water sample."

// true = watch labels show as pollution alerts (starts as DEMO_ALERTS in config.h)
extern bool watch_as_alert;

////////////////////////
// Function prototypes//
////////////////////////
struct Label_info get_label_info(enum Label_id id);
bool labels_match(enum Label_id a, enum Label_id b);
////////////////////////
