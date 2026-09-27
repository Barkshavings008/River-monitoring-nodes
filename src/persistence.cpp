#include "persistence.h"
#include "config.h"
#include <string.h>

void Persistence::reset() {
  memset(onStreak_, 0, sizeof(onStreak_));
  memset(offStreak_, 0, sizeof(offStreak_));
  memset(active_, 0, sizeof(active_));
  memset(conf_, 0, sizeof(conf_));
}

uint8_t Persistence::update(const RuleResult* hits, uint8_t n, AlertEvent* events, uint8_t maxEvents) {
  uint8_t nEv = 0;
  for (uint8_t id = LBL_NONE + 1; id < LBL_FAULT; id++) {
    float c = -1.0f;
    for (uint8_t i = 0; i < n; i++)
      if (hits[i].id == id) c = hits[i].conf;

    bool changed = false;
    if (c >= 0.0f) {
      offStreak_[id] = 0;
      if (onStreak_[id] < 255) onStreak_[id]++;
      conf_[id] = c;
      if (!active_[id] && onStreak_[id] >= PERSIST_ON_MIN) { active_[id] = true; changed = true; }
    } else {
      onStreak_[id] = 0;
      if (offStreak_[id] < 255) offStreak_[id]++;
      if (active_[id] && offStreak_[id] >= PERSIST_OFF_MIN) { active_[id] = false; changed = true; }
    }

    if (changed && nEv < maxEvents) {
      AlertEvent e = { (LabelId)id, active_[id], conf_[id] };
      events[nEv++] = e;
    }
  }
  return nEv;
}

bool Persistence::anyActive() const {
  for (uint8_t id = LBL_NONE + 1; id < LBL_FAULT; id++)
    if (active_[id]) return true;
  return false;
}
