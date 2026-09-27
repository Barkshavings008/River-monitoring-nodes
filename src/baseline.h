#pragma once
// Rolling "normal" for this spot: one ring buffer of slot medians per sensor.
#include "types.h"
#include "config.h"

// Median of v[0..n), sorts v in place.
float medianOf(float* v, uint16_t n);

class Baseline {
public:
  void reset();

  // Called once per minute with that minute's medians.
  // add[s] == false freezes sensor s (alert active, rain, or sensor fault).
  void addMinute(const float v[S_COUNT], const bool valid[S_COUNT], const bool add[S_COUNT]);

  bool ready() const { return learned_ >= BASELINE_MIN_MINUTES; }
  uint16_t learnedMinutes() const { return learned_; }
  bool has(uint8_t s) const { return count_[s] > 0; }
  float value(uint8_t s) const { return median_[s]; }

  // Rolling 24 h pH min/max. False until NUTRIENT_MIN_MINUTES of data.
  bool phDayRange(float& mn, float& mx) const;

  bool phDriftDetected() const { return drift_; }

private:
  float slots_[S_COUNT][BASELINE_SLOTS];
  uint16_t count_[S_COUNT];
  uint16_t head_[S_COUNT];
  float pending_[S_COUNT][BASELINE_SLOT_MIN];
  uint8_t pendingN_[S_COUNT];
  float median_[S_COUNT];
  uint16_t learned_;

  float hourMin_[PH_DAY_BUCKETS];
  float hourMax_[PH_DAY_BUCKETS];
  bool hourHas_[PH_DAY_BUCKETS];
  uint8_t hourIdx_;
  uint8_t minuteInHour_;
  uint32_t phTracked_;

  float driftHist_[DRIFT_SNAPSHOTS][S_COUNT];
  uint8_t driftN_;
  uint16_t minutesSinceSnap_;
  bool drift_;

  void recompute(uint8_t s);
  void trackPhDay(bool valid, float ph);
  void trackDrift();
  bool checkDrift() const;
};
