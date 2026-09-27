#include "rules.h"
#include "config.h"
#include <math.h>

namespace {

// Tallies required/optional conditions for one rule (section 6 confidence).
struct Score {
  uint8_t matched = 0, total = 0, strong = 0;
  bool requiredOk = true;

  void req(bool c) { total++; if (c) matched++; else requiredOk = false; }
  void opt(bool c) { total++; if (c) matched++; }
  void strongIf(bool c) { if (c) strong++; }

  RuleResult result(LabelId id) const {
    RuleResult r = { LBL_NONE, 0.0f };
    if (!requiredOk || total == 0) return r;
    float c = (float)matched / total + STRONG_BONUS * strong;
    r.id = id;
    r.conf = c > 1.0f ? 1.0f : c;
    return r;
  }
};

float baseOf(const RuleInputs& in, uint8_t s) {
  float b = in.base[s];
  if (s == S_TDS && b < BASE_FLOOR_TDS) b = BASE_FLOOR_TDS;
  if (s == S_NTU && b < BASE_FLOOR_NTU) b = BASE_FLOOR_NTU;
  return b;
}

// Fractional change vs baseline: +0.5 means 50 % above normal.
float rise(const RuleInputs& in, uint8_t s) { return (in.now[s] - baseOf(in, s)) / baseOf(in, s); }
float ratio(const RuleInputs& in, uint8_t s) { return in.now[s] / baseOf(in, s); }
float delta(const RuleInputs& in, uint8_t s) { return in.now[s] - in.base[s]; }

bool between(float v, float lo, float hi) { return v >= lo && v <= hi; }

void push(RuleOutput& out, RuleResult r) {
  if (r.id != LBL_NONE && out.n < MAX_RULE_HITS) out.hits[out.n++] = r;
}

}  // namespace

// ---------------------------------------------------------------- B. Rain filter
bool isRainEvent(const RuleInputs& in) {
  return in.ok[S_TDS] && in.ok[S_NTU] && in.ok[S_TEMP] &&
         rise(in, S_TDS) < -RAIN_TDS_DROP &&
         ratio(in, S_NTU) > RAIN_NTU_RATIO &&
         delta(in, S_TEMP) < -RAIN_TEMP_DROP;
}

// ---------------------------------------------------------------- C. Pollution
RuleResult ruleHeavyMetals(const RuleInputs& in) {
  Score s;
  float ph = in.now[S_PH];
  s.req(in.ok[S_PH] && (ph < HM_PH_MAX || -delta(in, S_PH) > HM_PH_DROP));
  s.req(in.ok[S_TDS] && rise(in, S_TDS) > HM_TDS_RISE);
  s.opt(in.ok[S_NTU] && ratio(in, S_NTU) > HM_NTU_RATIO);
  s.strongIf(in.ok[S_PH] && ph < HM_PH_STRONG);
  return s.result(LBL_HEAVY_METALS);
}

RuleResult ruleIndustrial(const RuleInputs& in) {
  Score s;
  float ph = in.now[S_PH];
  s.req(in.ok[S_PH] && (ph < IND_PH_LOW || ph > IND_PH_HIGH));
  // Step change: TDS jumped > 30 % within the step window, and is > 30 % over normal.
  s.req(in.ok[S_TDS] && in.hasTdsStepRef &&
        in.now[S_TDS] > in.tdsStepRef * (1.0f + IND_TDS_STEP) &&
        rise(in, S_TDS) > IND_TDS_STEP);
  s.opt(in.ok[S_TEMP] && delta(in, S_TEMP) > IND_TEMP_RISE);
  return s.result(LBL_INDUSTRIAL);
}

RuleResult ruleAlkaline(const RuleInputs& in) {
  Score s;
  float ph = in.now[S_PH];
  s.req(in.ok[S_PH] && ph > ALK_PH_MIN);
  s.req(in.ok[S_TDS] && rise(in, S_TDS) > ALK_TDS_RISE);
  s.opt(in.ok[S_NTU] && ratio(in, S_NTU) > ALK_NTU_RATIO);
  s.strongIf(in.ok[S_PH] && ph > ALK_PH_STRONG);
  return s.result(LBL_ALKALINE);
}

RuleResult ruleSewage(const RuleInputs& in) {
  Score s;
  s.req(in.ok[S_TDS] && between(rise(in, S_TDS), SEW_TDS_RISE_MIN, SEW_TDS_RISE_MAX));
  s.req(in.ok[S_NTU] && ratio(in, S_NTU) > SEW_NTU_RATIO);
  s.opt(in.ok[S_PH] && between(-delta(in, S_PH), SEW_PH_DROP_MIN, SEW_PH_DROP_MAX));
  s.opt(in.ok[S_TEMP] && between(delta(in, S_TEMP), SEW_TEMP_RISE_MIN, SEW_TEMP_RISE_MAX));
  return s.result(LBL_SEWAGE);
}

// ---------------------------------------------------------------- D. Watch
RuleResult ruleNutrients(const RuleInputs& in) {
  RuleResult none = { LBL_NONE, 0.0f };
  if (!in.hasDayRange) return none;
  Score s;
  // No real-time clock, so "daytime pH > 9" is treated as any pH > 9.
  s.req(in.ok[S_PH] && (in.phDayMax - in.phDayMin > NUT_PH_SWING || in.now[S_PH] > NUT_PH_HIGH));
  s.req(in.ok[S_TDS] && between(rise(in, S_TDS), NUT_TDS_RISE_MIN, NUT_TDS_RISE_MAX));
  s.opt(in.ok[S_TEMP] && in.now[S_TEMP] > NUT_TEMP_WARM);
  return s.result(LBL_NUTRIENTS);
}

RuleResult ruleThermal(const RuleInputs& in) {
  Score s;
  s.req(in.ok[S_TEMP] && (delta(in, S_TEMP) > THERM_TEMP_RISE || in.now[S_TEMP] > THERM_TEMP_ABS));
  bool othersNormal = in.ok[S_PH] && fabsf(delta(in, S_PH)) <= THERM_PH_TOL &&
                      in.ok[S_TDS] && fabsf(rise(in, S_TDS)) <= THERM_TDS_TOL &&
                      in.ok[S_NTU] && ratio(in, S_NTU) < THERM_NTU_RATIO;
  s.req(othersNormal);
  return s.result(LBL_THERMAL);
}

RuleResult ruleSediment(const RuleInputs& in) {
  Score s;
  s.req(in.ok[S_NTU] && (ratio(in, S_NTU) > SED_NTU_RATIO || in.now[S_NTU] > SED_NTU_ABS));
  s.req(in.ok[S_TDS] && fabsf(rise(in, S_TDS)) <= SED_TDS_TOL);
  s.req(in.ok[S_PH] && fabsf(delta(in, S_PH)) <= SED_PH_TOL);
  return s.result(LBL_SEDIMENT);
}

RuleResult ruleSalt(const RuleInputs& in) {
  Score s;
  s.req(in.ok[S_TDS] && in.now[S_TDS] > SALT_TDS_ABS);
  s.req(in.ok[S_PH] && fabsf(delta(in, S_PH)) <= SALT_PH_TOL);
  s.req(in.ok[S_NTU] && ratio(in, S_NTU) < SALT_NTU_RATIO);
  return s.result(LBL_SALT);
}

RuleOutput evaluateRules(const RuleInputs& in) {
  RuleOutput out;
  out.n = 0;
  out.rain = isRainEvent(in);

  if (out.rain) {
    RuleResult r = { LBL_RAIN, 1.0f };
    push(out, r);
  } else {
    push(out, ruleHeavyMetals(in));
    push(out, ruleIndustrial(in));
    push(out, ruleAlkaline(in));
    push(out, ruleSewage(in));
  }

  push(out, ruleNutrients(in));
  push(out, ruleThermal(in));
  if (!out.rain) push(out, ruleSediment(in));
  push(out, ruleSalt(in));
  return out;
}

// ---------------------------------------------------------------- A. Faults
void FaultTracker::reset() {
  for (uint8_t s = 0; s < S_COUNT; s++) {
    ref[s] = 0.0f;
    run[s] = 0;
    hasRef[s] = false;
  }
  prevTemp = 0.0f;
  hasPrevTemp = false;
}

FaultCode checkRange(uint8_t sensor, float v) {
  switch (sensor) {
    case S_PH:   return (v < RANGE_PH_MIN || v > RANGE_PH_MAX) ? F_OUT_OF_RANGE : F_NONE;
    case S_TDS:  return (v < RANGE_TDS_MIN || v > RANGE_TDS_MAX) ? F_OUT_OF_RANGE : F_NONE;
    case S_NTU:  return (v > RANGE_NTU_MAX) ? F_OUT_OF_RANGE : F_NONE;
    case S_TEMP: return (v < RANGE_TEMP_MIN || v > RANGE_TEMP_MAX) ? F_OUT_OF_RANGE : F_NONE;
  }
  return F_NONE;
}

void checkFaults(FaultTracker& ft, const float v[S_COUNT], const bool valid[S_COUNT], uint8_t out[S_COUNT]) {
  for (uint8_t s = 0; s < S_COUNT; s++) {
    out[s] = F_NONE;
    if (!valid[s]) {
      out[s] = F_NO_READING;
      ft.hasRef[s] = false;
      ft.run[s] = 0;
      continue;
    }

    // Flatline: same value (within 0.1 %) for FLATLINE_MIN minutes.
    float tol = fabsf(ft.ref[s]) * FLATLINE_TOL;
    if (tol < FLATLINE_ABS_TOL) tol = FLATLINE_ABS_TOL;
    bool clearWater = s == S_NTU && v[s] <= FLATLINE_NTU_FLOOR;
    if (ft.hasRef[s] && !clearWater && fabsf(v[s] - ft.ref[s]) <= tol) {
      if (ft.run[s] < 0xFFFF) ft.run[s]++;
    } else {
      ft.ref[s] = v[s];
      ft.hasRef[s] = true;
      ft.run[s] = 0;
    }

    FaultCode range = checkRange(s, v[s]);
    if (range != F_NONE) out[s] = range;
    else if (ft.run[s] >= FLATLINE_MIN) out[s] = F_FLATLINE;
  }

  if (valid[S_TEMP]) {
    if (ft.hasPrevTemp && out[S_TEMP] == F_NONE && fabsf(v[S_TEMP] - ft.prevTemp) > TEMP_JUMP_C)
      out[S_TEMP] = F_TEMP_JUMP;
    ft.prevTemp = v[S_TEMP];
    ft.hasPrevTemp = true;
  } else {
    ft.hasPrevTemp = false;
  }
}
