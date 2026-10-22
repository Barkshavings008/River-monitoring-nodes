#include "labels.h"

bool watch_as_alert = DEMO_ALERTS;

// Fills in the text for label id (anything unknown gets the "No pattern" text)
struct Label_info get_label_info(enum Label_id id) {
    struct Label_info info;

    if (id == LBL_HEAVY_METALS) {
        info.code = "LIKELY_HEAVY_METALS";
        info.name = "Likely heavy metals / acid mine drainage";
        info.short_name = "Likely heavy metals";
        info.category = CAT_POLLUTION;
        info.pollutants = "Iron, aluminium, manganese, copper, zinc, lead, cadmium, arsenic, "
                          "sulfate / sulfuric acid";
        info.meaning = "Acidic, metal-rich water. Low pH also pulls metals out of the riverbed.";
    } else if (id == LBL_INDUSTRIAL) {
        info.code = "LIKELY_INDUSTRIAL_DISCHARGE";
        info.name = "Likely industrial discharge";
        info.short_name = "Likely industrial discharge";
        info.category = CAT_POLLUTION;
        info.pollutants = "Acids or alkalis, dissolved salts, plating metals (chromium, nickel), "
                          "dyes, cleaning chemicals";
        info.meaning = "Leak from a single point; sudden jump in readings.";
    } else if (id == LBL_ALKALINE) {
        info.code = "LIKELY_ALKALINE_WASTE";
        info.name = "Likely alkaline waste";
        info.short_name = "Likely alkaline waste";
        info.category = CAT_POLLUTION;
        info.pollutants = "Cement dust, lime (calcium hydroxide), caustic soda, strong detergents, "
                          "chromium traces";
        info.meaning = "Usually construction-site runoff.";
    } else if (id == LBL_SEWAGE) {
        info.code = "LIKELY_SEWAGE";
        info.name = "Likely sewage";
        info.short_name = "Likely sewage";
        info.category = CAT_POLLUTION;
        info.pollutants = "Ammonia, nitrates, phosphates, E. coli and other bacteria, organic matter, "
                          "detergents, medicine residues";
        info.meaning = "Untreated wastewater, often during heavy rain.";
    } else if (id == LBL_NUTRIENTS) {
        info.code = "LIKELY_NUTRIENTS";
        info.name = "Likely nutrient pollution / algae";
        info.short_name = "Likely nutrients";
        info.category = CAT_WATCH;
        info.pollutants = "Nitrate, phosphate, ammonia (fertiliser, manure, sewage); "
                          "risk of toxic blue-green algae";
        info.meaning = "Algae growth causing a large day-night pH swing.";
    } else if (id == LBL_THERMAL) {
        info.code = "THERMAL_POLLUTION";
        info.name = "Thermal pollution";
        info.short_name = "Thermal pollution";
        info.category = CAT_WATCH;
        info.pollutants = "Heated cooling water; sometimes chlorine or anti-algae chemicals";
        info.meaning = "Warm outflow; warm water holds less oxygen for fish.";
    } else if (id == LBL_SEDIMENT) {
        info.code = "LIKELY_SEDIMENT";
        info.name = "Likely sediment";
        info.short_name = "Likely sediment";
        info.category = CAT_WATCH;
        info.pollutants = "Silt, clay, plus attached phosphorus, pesticides, metals";
        info.meaning = "Land clearing or bank collapse upstream, no rain.";
    } else if (id == LBL_SALT) {
        info.code = "LIKELY_SALT";
        info.name = "Likely salt / saline water";
        info.short_name = "Likely salt";
        info.category = CAT_WATCH;
        info.pollutants = "Sodium chloride; seawater magnesium and sulfate; salty groundwater";
        info.meaning = "Tide pushing upstream, road salt, or salty groundwater.";
    } else if (id == LBL_EFFLUENT) {
        info.code = "LIKELY_EFFLUENT_OR_FERTILISER";
        info.name = "Likely treated wastewater or fertiliser runoff";
        info.short_name = "Likely effluent / fertiliser";
        info.category = CAT_WATCH;
        info.pollutants = "Nitrate, ammonium, phosphate, dissolved salts (chloride, sulfate), "
                          "detergent and medicine residues";
        info.meaning = "Clear water carrying extra dissolved salts and a slightly lower pH: treated "
                       "sewage or fertiliser washing in. Can feed algae if it keeps up.";
    } else if (id == LBL_TURBIDITY) {
        info.code = "TURBIDITY_RISE";
        info.name = "Turbidity spike (only turbidity measured here)";
        info.short_name = "Turbidity spike";
        info.category = CAT_WATCH;
        info.pollutants = "Most likely mud or sediment; could also be sewage or other cloudy waste "
                          "(this node can't measure pH or dissolved salts)";
        info.meaning = "The water is much cloudier than normal. See what the nodes with all four "
                       "sensors found.";
    } else if (id == LBL_RAIN) {
        info.code = "LIKELY_RAIN_EVENT";
        info.name = "Likely rain / stormwater event";
        info.short_name = "Likely rain event";
        info.category = CAT_FILTER;
        info.pollutants = "Not a leak, but first flush carries road oil, tyre-dust zinc, litter, "
                          "microplastics, bacteria";
        info.meaning = "Normal weather; should affect all nodes at once.";
    } else if (id == LBL_FAULT) {
        info.code = "FAULT";
        info.name = "Sensor fault";
        info.short_name = "Sensor fault";
        info.category = CAT_FAULT;
        info.pollutants = "None (hardware issue)";
        info.meaning = "Check the named sensor, wiring or calibration.";
    } else {
        info.code = "NONE";
        info.name = "No pattern";
        info.short_name = "No pattern";
        info.category = CAT_NONE;
        info.pollutants = "";
        info.meaning = "";
    }

    // For demos: watch labels (salt, sediment, warm water...) show as full alerts
    info.base_category = info.category;
    if (watch_as_alert && info.category == CAT_WATCH) {
        info.category = CAT_POLLUTION;
    }
    return info;
}

// true if two nodes are probably seeing the same thing. A node that only
// measures turbidity can't name the cause, so its "turbidity spike" matches
// the muddy-water labels of nodes with all four sensors (sediment, sewage).
bool labels_match(enum Label_id a, enum Label_id b) {
    if (a == b) {
        return true;
    }
    bool a_muddy = a == LBL_TURBIDITY || a == LBL_SEDIMENT || a == LBL_SEWAGE;
    bool b_muddy = b == LBL_TURBIDITY || b == LBL_SEDIMENT || b == LBL_SEWAGE;
    return a_muddy && b_muddy && (a == LBL_TURBIDITY || b == LBL_TURBIDITY);
}
