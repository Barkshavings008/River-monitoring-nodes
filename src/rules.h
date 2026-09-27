#pragma once
// Fault checks and pattern rules. Pure functions: no Serial, no Arduino,
// so they can be unit tested on a PC with fake data.
#include "types.h"

struct RuleInputs {
    float now[S_COUNT];        // this minute's medians
    float base[S_COUNT];       // baseline medians
    bool ok[S_COUNT];          // valid, not faulted, baseline available
    bool hasTdsStepRef;
    float tdsStepRef;          // TDS up to STEP_WINDOW_MIN ago
                               // (industrial step change)
    bool hasDayRange;          // >= 12 h of pH data and not demo mode
    float phDayMin;
    float phDayMax;
};

const uint8_t MAX_RULE_HITS = 10;

struct RuleOutput {
    RuleResult hits[MAX_RULE_HITS];   // every label whose required
                                      // conditions are met
    uint8_t n;
    bool rain;                        // B fired: C rules and sediment
                                      // were skipped
};

// Section 4 B/C/D, run on 1-minute values.
RuleOutput evaluateRules(const RuleInputs &in);

bool isRainEvent(const RuleInputs &in);
RuleResult ruleHeavyMetals(const RuleInputs &in);
RuleResult ruleIndustrial(const RuleInputs &in);
RuleResult ruleAlkaline(const RuleInputs &in);
RuleResult ruleSewage(const RuleInputs &in);
RuleResult ruleNutrients(const RuleInputs &in);
RuleResult ruleThermal(const RuleInputs &in);
RuleResult ruleSediment(const RuleInputs &in);
RuleResult ruleSalt(const RuleInputs &in);

// Section 4 A. Keeps per-sensor history between minutes.
struct FaultTracker {
    float ref[S_COUNT];
    uint16_t run[S_COUNT];
    bool hasRef[S_COUNT];
    float prevTemp;
    bool hasPrevTemp;
    void reset();
};

FaultCode checkRange(uint8_t sensor, float v);

// Writes one FaultCode per sensor into out[].
void checkFaults(FaultTracker &ft, const float v[S_COUNT],
                 const bool valid[S_COUNT], uint8_t out[S_COUNT]);
