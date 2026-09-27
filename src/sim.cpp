#include "sim.h"
#include <string.h>

namespace {

struct Profile { float ph, tds, ntu, temp; };

Profile normalFor(char role) {
  switch (role) {
    case SIM_UPSTREAM:   return { 7.30f, 200.0f, 7.0f, 17.0f };
    case SIM_SIBLING:    return { 7.10f, 215.0f, 8.5f, 17.2f };
    case SIM_DOWNSTREAM: return { 7.20f, 220.0f, 9.0f, 17.3f };
    default:             return { 7.20f, 210.0f, 8.1f, 17.1f };
  }
}

// Deterministic noise in [-1, 1] so runs (and tests) are repeatable.
float noise(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t x = a * 73856093u ^ b * 19349663u ^ c * 83492791u;
  x ^= x >> 13;
  x *= 0x5bd1e995u;
  x ^= x >> 15;
  return (x & 0xFFFF) / 32767.5f - 1.0f;
}

}  // namespace

Reading simReading(char role, uint32_t minute, uint32_t sample) {
  Profile p = normalFor(role);
  uint32_t cycle = minute / SIM_CYCLE_MIN;
  uint32_t c = minute % SIM_CYCLE_MIN;

  bool rain = c >= SIM_RAIN_START && c < SIM_RAIN_END && (role == SIM_LOCAL || cycle % 2 == 0);
  if (rain) {
    p.tds *= 0.80f;
    p.ntu *= 3.2f;
    p.temp -= 0.8f;
  }
  if (role == SIM_LOCAL && c >= SIM_ACID_START && c < SIM_ACID_END) {
    p.ph = 4.6f; p.tds = 612.0f; p.ntu = 18.2f; p.temp += 0.3f;
  }
  if (role == SIM_DOWNSTREAM && c >= SIM_ACID_START + SIM_DOWNSTREAM_DELAY &&
      c < SIM_ACID_END + SIM_DOWNSTREAM_DELAY) {
    p.ph = 5.6f; p.tds = 400.0f; p.ntu = 12.0f;   // diluted plume
  }

  uint32_t seed = minute * 64 + sample;
  Reading r;
  r.v[S_PH]   = p.ph   + 0.03f * noise(seed, role, 1);
  r.v[S_TDS]  = p.tds  * (1.0f + 0.015f * noise(seed, role, 2));
  r.v[S_NTU]  = p.ntu  * (1.0f + 0.03f * noise(seed, role, 3));
  r.v[S_TEMP] = p.temp + 0.04f * noise(seed, role, 4);
  for (uint8_t s = 0; s < S_COUNT; s++) r.valid[s] = true;
  return r;
}

void SimRemotes::begin() {
  n_ = 0;
  for (uint8_t i = 0; i < DEFAULT_LAYOUT_COUNT && n_ < MAX_SIM_REMOTES; i++) {
    const NodeLayout& l = DEFAULT_LAYOUT[i];
    if (l.isLocal || !l.simRole) continue;
    Entry& e = entries_[n_++];
    strncpy(e.id, l.id, NODE_ID_LEN - 1);
    e.id[NODE_ID_LEN - 1] = '\0';
    e.role = l.simRole;
    e.engine.begin(l.id);
  }
}

void SimRemotes::tick(uint32_t simMinute, uint32_t uptimeSec, RiverNetwork& net, uint32_t nowMin) {
  for (uint8_t i = 0; i < n_; i++) {
    Entry& e = entries_[i];
    if (net.indexOf(e.id) < 0) continue;   // node was deleted from the network
    for (uint8_t k = 0; k < SIM_SAMPLES_PER_MIN; k++)
      e.engine.addSample(simReading(e.role, simMinute, k));
    NodeReport rep;
    AlertEvent ev[4];
    uint8_t nEv;
    e.engine.closeMinute(uptimeSec, rep, ev, 4, nEv);
    net.updateReport(e.id, rep, nowMin);
  }
}
