#pragma once
// Text that goes with each label. The rules only return the Label_id, the
// printing code uses get_label_info() to get the words.
#include "types.h"

struct Label_info {
    const char *code;         // machine name, e.g. LIKELY_HEAVY_METALS
    const char *name;         // long human name
    const char *short_name;   // for the ALERT ON/OFF lines
    enum Category category;
    const char *pollutants;
    const char *meaning;
};

#define DISCLAIMER_LINE1 "Pollutants listed are likely candidates only."
#define DISCLAIMER_LINE2 "Confirm with a lab water sample."

////////////////////////
// Function prototypes//
////////////////////////
struct Label_info get_label_info(enum Label_id id);
////////////////////////
