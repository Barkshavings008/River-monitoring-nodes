#pragma once
// SIMULATE mode: scripted fake readings (normal -> rain -> normal -> acid spike),
// plus simulated neighbour nodes that run the same NodeEngine as the real node.
#include "types.h"
#include "config.h"
#include "node_engine.h"
#include "network.h"

const char SIM_LOCAL      = 'L';
const char SIM_UPSTREAM   = 'U';
const char SIM_SIBLING    = 'S';
const char SIM_DOWNSTREAM = 'D';

// Script, in minutes within a repeating cycle. Even cycles: rain at every
// node (confirmed). Odd cycles: rain only at the local node (unconfirmed).
const uint32_t SIM_CYCLE_MIN  = 45;
const uint32_t SIM_RAIN_START = 8,  SIM_RAIN_END = 15;
const uint32_t SIM_ACID_START = 20, SIM_ACID_END = 33;
const uint32_t SIM_DOWNSTREAM_DELAY = 2;
const uint8_t  SIM_SAMPLES_PER_MIN  = 5;

Reading simReading(char role, uint32_t minute, uint32_t sample);

const uint8_t MAX_SIM_REMOTES = 4;

class SimRemotes {
public:
  void begin();   // one simulated engine per DEFAULT_LAYOUT row with a simRole
  void tick(uint32_t simMinute, uint32_t uptimeSec, RiverNetwork& net, uint32_t nowMin);

private:
  struct Entry {
    char id[NODE_ID_LEN];
    char role;
    NodeEngine engine;
  };
  Entry entries_[MAX_SIM_REMOTES];
  uint8_t n_ = 0;
};
