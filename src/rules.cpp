#include "rules.h"
#include <math.h>

// Keeps count of a rule's required and optional conditions (section 6 confidence)
struct Score {
    int matched;
    int total;
    int strong;
    bool required_ok;
};

////////////////////////
// Function prototypes//
////////////////////////
struct Score new_score(void);
void score_required(struct Score *score, bool condition);
void score_optional(struct Score *score, bool condition);
void score_strong(struct Score *score, bool condition);
struct Rule_result score_result(struct Score score, enum Label_id id);
float safe_base(struct Rule_inputs in, int sensor);
float fraction_rise(struct Rule_inputs in, int sensor);
float times_base(struct Rule_inputs in, int sensor);
float change_from_base(struct Rule_inputs in, int sensor);
bool is_between(float value, float low, float high);
void add_hit(struct Rule_output *out, struct Rule_result result);
////////////////////////

//////////////////////////
////// B. RAIN FILTER ////
//////////////////////////

bool is_rain_event(struct Rule_inputs in) {
    return in.ok[S_TDS] && in.ok[S_NTU] && in.ok[S_TEMP] &&
           fraction_rise(in, S_TDS) < -RAIN_TDS_DROP &&
           times_base(in, S_NTU) > RAIN_NTU_RATIO &&
           change_from_base(in, S_TEMP) < -RAIN_TEMP_DROP;
}

//////////////////////////
////// C. POLLUTION //////
//////////////////////////

struct Rule_result rule_heavy_metals(struct Rule_inputs in) {
    struct Score score = new_score();
    float ph = in.now[S_PH];
    score_required(&score, in.ok[S_PH] && (ph < HM_PH_MAX || -change_from_base(in, S_PH) > HM_PH_DROP));
    score_required(&score, in.ok[S_TDS] && fraction_rise(in, S_TDS) > HM_TDS_RISE);
    score_optional(&score, in.ok[S_NTU] && times_base(in, S_NTU) > HM_NTU_RATIO);
    score_strong(&score, in.ok[S_PH] && ph < HM_PH_STRONG);
    return score_result(score, LBL_HEAVY_METALS);
}

struct Rule_result rule_industrial(struct Rule_inputs in) {
    struct Score score = new_score();
    float ph = in.now[S_PH];
    score_required(&score, in.ok[S_PH] && (ph < IND_PH_LOW || ph > IND_PH_HIGH));
    // Step change: TDS jumped > 30 % within the step window, and is > 30 % over normal
    score_required(&score, in.ok[S_TDS] && in.has_tds_step_ref &&
                           in.now[S_TDS] > in.tds_step_ref * (1.0f + IND_TDS_STEP) &&
                           fraction_rise(in, S_TDS) > IND_TDS_STEP);
    score_optional(&score, in.ok[S_TEMP] && change_from_base(in, S_TEMP) > IND_TEMP_RISE);
    return score_result(score, LBL_INDUSTRIAL);
}

struct Rule_result rule_alkaline(struct Rule_inputs in) {
    struct Score score = new_score();
    float ph = in.now[S_PH];
    score_required(&score, in.ok[S_PH] && ph > ALK_PH_MIN);
    score_required(&score, in.ok[S_TDS] && fraction_rise(in, S_TDS) > ALK_TDS_RISE);
    score_optional(&score, in.ok[S_NTU] && times_base(in, S_NTU) > ALK_NTU_RATIO);
    score_strong(&score, in.ok[S_PH] && ph > ALK_PH_STRONG);
    return score_result(score, LBL_ALKALINE);
}

struct Rule_result rule_sewage(struct Rule_inputs in) {
    struct Score score = new_score();
    score_required(&score, in.ok[S_TDS] &&
                           is_between(fraction_rise(in, S_TDS), SEW_TDS_RISE_MIN, SEW_TDS_RISE_MAX));
    score_required(&score, in.ok[S_NTU] && times_base(in, S_NTU) > SEW_NTU_RATIO);
    score_optional(&score, in.ok[S_PH] &&
                           is_between(-change_from_base(in, S_PH), SEW_PH_DROP_MIN, SEW_PH_DROP_MAX));
    score_optional(&score, in.ok[S_TEMP] &&
                           is_between(change_from_base(in, S_TEMP), SEW_TEMP_RISE_MIN, SEW_TEMP_RISE_MAX));
    return score_result(score, LBL_SEWAGE);
}

//////////////////////////
//////// D. WATCH ////////
//////////////////////////

struct Rule_result rule_nutrients(struct Rule_inputs in) {
    if (!in.has_day_range) {
        struct Rule_result none;
        none.id = LBL_NONE;
        none.confidence = 0.0f;
        return none;
    }
    struct Score score = new_score();
    // No real-time clock, so "daytime pH > 9" is treated as any pH > 9
    float swing = in.ph_day_max - in.ph_day_min;
    score_required(&score, in.ok[S_PH] && (swing > NUT_PH_SWING || in.now[S_PH] > NUT_PH_HIGH));
    score_required(&score, in.ok[S_TDS] &&
                           is_between(fraction_rise(in, S_TDS), NUT_TDS_RISE_MIN, NUT_TDS_RISE_MAX));
    score_optional(&score, in.ok[S_TEMP] && in.now[S_TEMP] > NUT_TEMP_WARM);
    return score_result(score, LBL_NUTRIENTS);
}

struct Rule_result rule_thermal(struct Rule_inputs in) {
    struct Score score = new_score();
    score_required(&score, in.ok[S_TEMP] && (change_from_base(in, S_TEMP) > THERM_TEMP_RISE ||
                                             in.now[S_TEMP] > THERM_TEMP_ABS));
    bool others_normal = in.ok[S_PH] && fabsf(change_from_base(in, S_PH)) <= THERM_PH_TOL &&
                         in.ok[S_TDS] && fabsf(fraction_rise(in, S_TDS)) <= THERM_TDS_TOL &&
                         in.ok[S_NTU] && times_base(in, S_NTU) < THERM_NTU_RATIO;
    score_required(&score, others_normal);
    return score_result(score, LBL_THERMAL);
}

struct Rule_result rule_sediment(struct Rule_inputs in) {
    struct Score score = new_score();
    score_required(&score, in.ok[S_NTU] && (times_base(in, S_NTU) > SED_NTU_RATIO ||
                                            in.now[S_NTU] > SED_NTU_ABS));
    score_required(&score, in.ok[S_TDS] && fabsf(fraction_rise(in, S_TDS)) <= SED_TDS_TOL);
    score_required(&score, in.ok[S_PH] && fabsf(change_from_base(in, S_PH)) <= SED_PH_TOL);
    return score_result(score, LBL_SEDIMENT);
}

struct Rule_result rule_salt(struct Rule_inputs in) {
    struct Score score = new_score();
    score_required(&score, in.ok[S_TDS] && in.now[S_TDS] > SALT_TDS_ABS);
    score_required(&score, in.ok[S_PH] && fabsf(change_from_base(in, S_PH)) <= SALT_PH_TOL);
    score_required(&score, in.ok[S_NTU] && times_base(in, S_NTU) < SALT_NTU_RATIO);
    return score_result(score, LBL_SALT);
}

// Clear water, more dissolved salts and a slightly lower pH. Treated sewage
// and fertiliser both add nitrate/ammonium and salts (which lower the pH a
// little) without making the water muddy. Muddy water is the sewage rule.
struct Rule_result rule_effluent(struct Rule_inputs in) {
    struct Score score = new_score();
    score_required(&score, in.ok[S_TDS] && fraction_rise(in, S_TDS) > EFF_TDS_RISE);
    score_required(&score, in.ok[S_PH] &&
                           is_between(-change_from_base(in, S_PH), EFF_PH_DROP_MIN, EFF_PH_DROP_MAX));
    score_required(&score, in.ok[S_NTU] && times_base(in, S_NTU) < EFF_NTU_RATIO);
    score_optional(&score, in.ok[S_TEMP] &&
                           is_between(change_from_base(in, S_TEMP), EFF_TEMP_RISE_MIN, EFF_TEMP_RISE_MAX));
    return score_result(score, LBL_EFFLUENT);
}

// Runs every rule. Rain skips the pollution rules and sediment, because
// rain by itself explains those readings.
struct Rule_output evaluate_rules(struct Rule_inputs in) {
    struct Rule_output out;
    out.num_hits = 0;
    out.rain = is_rain_event(in);

    if (out.rain) {
        struct Rule_result rain;
        rain.id = LBL_RAIN;
        rain.confidence = 1.0f;
        add_hit(&out, rain);
    } else {
        add_hit(&out, rule_heavy_metals(in));
        add_hit(&out, rule_industrial(in));
        add_hit(&out, rule_alkaline(in));
        add_hit(&out, rule_sewage(in));
    }

    add_hit(&out, rule_nutrients(in));
    add_hit(&out, rule_thermal(in));
    if (!out.rain) {
        add_hit(&out, rule_sediment(in));
    }
    add_hit(&out, rule_salt(in));
    add_hit(&out, rule_effluent(in));
    return out;
}

//////////////////////////
/////// A. FAULTS ////////
//////////////////////////

void fault_tracker_reset(struct Fault_tracker *tracker) {
    for (int s = 0; s < S_COUNT; s++) {
        tracker->ref[s] = 0.0f;
        tracker->run[s] = 0;
        tracker->has_ref[s] = false;
    }
    tracker->prev_temp = 0.0f;
    tracker->has_prev_temp = false;
}

// F_OUT_OF_RANGE if the value is impossible for that sensor
enum Fault_code check_range(int sensor, float value) {
    bool out_of_range = false;
    if (sensor == S_PH) {
        out_of_range = value < RANGE_PH_MIN || value > RANGE_PH_MAX;
    } else if (sensor == S_TDS) {
        out_of_range = value < RANGE_TDS_MIN || value > RANGE_TDS_MAX;
    } else if (sensor == S_NTU) {
        out_of_range = value > RANGE_NTU_MAX;
    } else if (sensor == S_TEMP) {
        out_of_range = value < RANGE_TEMP_MIN || value > RANGE_TEMP_MAX;
    }

    if (out_of_range) {
        return F_OUT_OF_RANGE;
    } else {
        return F_NONE;
    }
}

// Writes one Fault_code for each sensor into faults[]
void check_faults(struct Fault_tracker *tracker, float values[], bool valid[], enum Fault_code faults[]) {
    for (int s = 0; s < S_COUNT; s++) {
        faults[s] = F_NONE;
        if (!valid[s]) {
            faults[s] = F_NO_READING;
            tracker->has_ref[s] = false;
            tracker->run[s] = 0;
        } else {
            // Flatline: same value (within 0.1 %) for FLATLINE_MIN minutes
            float tolerance = fabsf(tracker->ref[s]) * FLATLINE_TOL;
            if (tolerance < FLATLINE_ABS_TOL) {
                tolerance = FLATLINE_ABS_TOL;
            }
            bool clear_water = s == S_NTU && values[s] <= FLATLINE_NTU_FLOOR;
            bool same = fabsf(values[s] - tracker->ref[s]) <= tolerance;
            if (tracker->has_ref[s] && !clear_water && same) {
                if (tracker->run[s] < 0xFFFF) {
                    tracker->run[s]++;
                }
            } else {
                tracker->ref[s] = values[s];
                tracker->has_ref[s] = true;
                tracker->run[s] = 0;
            }

            enum Fault_code range = check_range(s, values[s]);
            if (range != F_NONE) {
                faults[s] = range;
            } else if (tracker->run[s] >= FLATLINE_MIN) {
                faults[s] = F_FLATLINE;
            }
        }
    }

    // Temperature jump: moved more than TEMP_JUMP_C since last minute
    if (valid[S_TEMP]) {
        if (tracker->has_prev_temp && faults[S_TEMP] == F_NONE &&
            fabsf(values[S_TEMP] - tracker->prev_temp) > TEMP_JUMP_C) {
            faults[S_TEMP] = F_TEMP_JUMP;
        }
        tracker->prev_temp = values[S_TEMP];
        tracker->has_prev_temp = true;
    } else {
        tracker->has_prev_temp = false;
    }
}

/////////////////////////
// Function definitions//
/////////////////////////

struct Score new_score(void) {
    struct Score score;
    score.matched = 0;
    score.total = 0;
    score.strong = 0;
    score.required_ok = true;
    return score;
}

// A condition that has to be true for the rule to fire
void score_required(struct Score *score, bool condition) {
    score->total++;
    if (condition) {
        score->matched++;
    } else {
        score->required_ok = false;
    }
}

// A condition that only adds to the confidence
void score_optional(struct Score *score, bool condition) {
    score->total++;
    if (condition) {
        score->matched++;
    }
}

// A really strong sign, adds STRONG_BONUS to the confidence
void score_strong(struct Score *score, bool condition) {
    if (condition) {
        score->strong++;
    }
}

// The label and its confidence, or LBL_NONE if a required condition failed
struct Rule_result score_result(struct Score score, enum Label_id id) {
    struct Rule_result result;
    result.id = LBL_NONE;
    result.confidence = 0.0f;
    if (!score.required_ok || score.total == 0) {
        return result;
    }
    float confidence = (float)score.matched / score.total + STRONG_BONUS * score.strong;
    if (confidence > 1.0f) {
        confidence = 1.0f;
    }
    result.id = id;
    result.confidence = confidence;
    return result;
}

// Baseline of a sensor, but never below the floor (so it's safe to divide by)
float safe_base(struct Rule_inputs in, int sensor) {
    float base = in.base[sensor];
    if (sensor == S_TDS && base < BASE_FLOOR_TDS) {
        base = BASE_FLOOR_TDS;
    }
    if (sensor == S_NTU && base < BASE_FLOOR_NTU) {
        base = BASE_FLOOR_NTU;
    }
    return base;
}

// Change compared to normal as a fraction: +0.5 means 50 % above normal
float fraction_rise(struct Rule_inputs in, int sensor) {
    return (in.now[sensor] - safe_base(in, sensor)) / safe_base(in, sensor);
}

// How many times bigger than normal the reading is
float times_base(struct Rule_inputs in, int sensor) {
    return in.now[sensor] / safe_base(in, sensor);
}

// Reading minus normal
float change_from_base(struct Rule_inputs in, int sensor) {
    return in.now[sensor] - in.base[sensor];
}

bool is_between(float value, float low, float high) {
    return value >= low && value <= high;
}

// Adds the result to the list if the rule fired and there's room
void add_hit(struct Rule_output *out, struct Rule_result result) {
    if (result.id != LBL_NONE && out->num_hits < MAX_RULE_HITS) {
        out->hits[out->num_hits] = result;
        out->num_hits++;
    }
}
