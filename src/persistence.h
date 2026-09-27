#pragma once
// Section 5: a label turns on after PERSIST_ON_MIN true minutes in a row,
// and off after PERSIST_OFF_MIN false minutes in a row.
#include "types.h"

struct AlertEvent {
  LabelId id;
  bool on;
  float conf;
};

class Persistence {
public:
  void reset();

  // hits: this minute's raw rule results. Writes ON/OFF transitions to events.
  uint8_t update(const RuleResult* hits, uint8_t n, AlertEvent* events, uint8_t maxEvents);

  bool active(LabelId id) const { return active_[id]; }
  float conf(LabelId id) const { return conf_[id]; }
  bool anyActive() const;

private:
  uint8_t onStreak_[LBL_COUNT];
  uint8_t offStreak_[LBL_COUNT];
  bool active_[LBL_COUNT];
  float conf_[LBL_COUNT];
};
