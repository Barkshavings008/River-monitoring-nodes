#pragma once
// Label info table used by the printer. Rules only return the LabelId.
// Row order must match the LabelId enum.
#include "types.h"

struct LabelInfo {
    LabelId id;
    const char *code;        // machine name, e.g. LIKELY_HEAVY_METALS
    const char *name;        // long human name
    const char *shortName;   // for ALERT ON/OFF lines
    Category category;
    const char *pollutants;
    const char *meaning;
};

static const LabelInfo LABELS[LBL_COUNT] = {
    {
        LBL_NONE, "NONE",
        "No pattern", "No pattern", CAT_NONE,
        "",
        ""
    },
    {
        LBL_HEAVY_METALS, "LIKELY_HEAVY_METALS",
        "Likely heavy metals / acid mine drainage", "Likely heavy metals",
        CAT_POLLUTION,
        "Iron, aluminium, manganese, copper, zinc, lead, cadmium, arsenic, "
        "sulfate / sulfuric acid",
        "Acidic, metal-rich water. Low pH also pulls metals out of the "
        "riverbed."
    },
    {
        LBL_INDUSTRIAL, "LIKELY_INDUSTRIAL_DISCHARGE",
        "Likely industrial discharge", "Likely industrial discharge",
        CAT_POLLUTION,
        "Acids or alkalis, dissolved salts, plating metals (chromium, "
        "nickel), dyes, cleaning chemicals",
        "Leak from a single point; sudden jump in readings."
    },
    {
        LBL_ALKALINE, "LIKELY_ALKALINE_WASTE",
        "Likely alkaline waste", "Likely alkaline waste", CAT_POLLUTION,
        "Cement dust, lime (calcium hydroxide), caustic soda, strong "
        "detergents, chromium traces",
        "Usually construction-site runoff."
    },
    {
        LBL_SEWAGE, "LIKELY_SEWAGE",
        "Likely sewage", "Likely sewage", CAT_POLLUTION,
        "Ammonia, nitrates, phosphates, E. coli and other bacteria, "
        "organic matter, detergents, medicine residues",
        "Untreated wastewater, often during heavy rain."
    },
    {
        LBL_NUTRIENTS, "LIKELY_NUTRIENTS",
        "Likely nutrient pollution / algae", "Likely nutrients", CAT_WATCH,
        "Nitrate, phosphate, ammonia (fertiliser, manure, sewage); risk of "
        "toxic blue-green algae",
        "Algae growth causing a large day-night pH swing."
    },
    {
        LBL_THERMAL, "THERMAL_POLLUTION",
        "Thermal pollution", "Thermal pollution", CAT_WATCH,
        "Heated cooling water; sometimes chlorine or anti-algae chemicals",
        "Warm outflow; warm water holds less oxygen for fish."
    },
    {
        LBL_SEDIMENT, "LIKELY_SEDIMENT",
        "Likely sediment", "Likely sediment", CAT_WATCH,
        "Silt, clay, plus attached phosphorus, pesticides, metals",
        "Land clearing or bank collapse upstream, no rain."
    },
    {
        LBL_SALT, "LIKELY_SALT",
        "Likely salt / saline water", "Likely salt", CAT_WATCH,
        "Sodium chloride; seawater magnesium and sulfate; salty groundwater",
        "Tide pushing upstream, road salt, or salty groundwater."
    },
    {
        LBL_RAIN, "LIKELY_RAIN_EVENT",
        "Likely rain / stormwater event", "Likely rain event", CAT_FILTER,
        "Not a leak, but first flush carries road oil, tyre-dust zinc, "
        "litter, microplastics, bacteria",
        "Normal weather; should affect all nodes at once."
    },
    {
        LBL_FAULT, "FAULT",
        "Sensor fault", "Sensor fault", CAT_FAULT,
        "None (hardware issue)",
        "Check the named sensor, wiring or calibration."
    },
};

static const char *const DISCLAIMER_LINE1 =
    "Pollutants listed are likely candidates only.";
static const char *const DISCLAIMER_LINE2 =
    "Confirm with a lab water sample.";

// Returns the info row for label id (the NONE row if id is out of range).
inline const LabelInfo &labelInfo(LabelId id) {
    if (id < LBL_COUNT) {
        return LABELS[id];
    }
    return LABELS[LBL_NONE];
}
