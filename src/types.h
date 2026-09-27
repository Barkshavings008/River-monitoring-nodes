#pragma once
// Plain data types shared by every module (no Arduino dependency).
#include <stdint.h>

enum Sensor : uint8_t { S_PH = 0, S_TDS, S_NTU, S_TEMP, S_COUNT };

struct Reading {
  float v[S_COUNT];
  bool valid[S_COUNT];
};

enum LabelId : uint8_t {
  LBL_NONE = 0,
  LBL_HEAVY_METALS, LBL_INDUSTRIAL, LBL_ALKALINE, LBL_SEWAGE,   // C: pollution
  LBL_NUTRIENTS, LBL_THERMAL, LBL_SEDIMENT, LBL_SALT,           // D: watch
  LBL_RAIN,                                                     // B: filter
  LBL_FAULT,
  LBL_COUNT
};

enum Category : uint8_t { CAT_NONE, CAT_POLLUTION, CAT_WATCH, CAT_FILTER, CAT_FAULT };

enum FaultCode : uint8_t { F_NONE, F_NO_READING, F_OUT_OF_RANGE, F_FLATLINE, F_TEMP_JUMP };

enum NodeState : uint8_t { ST_BASELINE_BUILDING, ST_NORMAL, ST_WATCH, ST_ALERT, ST_FAULT };

struct RuleResult {
  LabelId id;
  float conf;
};

const uint8_t MAX_ALSO = 8;

// One node's minute summary: what gets printed, and what a node would send
// to its neighbours over LoRa/WiFi.
struct NodeReport {
  char id[8];
  uint32_t t;                  // seconds since node boot
  NodeState state;
  LabelId label;
  float conf;
  LabelId also[MAX_ALSO];
  uint8_t nAlso;
  float now[S_COUNT];
  bool nowValid[S_COUNT];
  float base[S_COUNT];
  bool baseValid[S_COUNT];
  uint8_t fault[S_COUNT];      // FaultCode per sensor
  bool phRecalibrate;
  uint16_t learnedMin;
  uint16_t learnNeeded;
  bool rainPattern;            // raw rain pattern this minute (before persistence)
};

inline const char* sensorName(uint8_t s) {
  static const char* const names[S_COUNT] = { "pH", "TDS", "Turbidity", "Water temp" };
  return s < S_COUNT ? names[s] : "?";
}

inline const char* sensorCode(uint8_t s) {
  static const char* const codes[S_COUNT] = { "PH", "TDS", "NTU", "TEMP" };
  return s < S_COUNT ? codes[s] : "?";
}

inline const char* stateName(NodeState st) {
  static const char* const names[] = { "BASELINE_BUILDING", "NORMAL", "WATCH", "ALERT", "FAULT" };
  return names[st];
}

inline bool reportHasLabel(const NodeReport& r, LabelId id) {
  if (r.label == id) return true;
  for (uint8_t i = 0; i < r.nAlso; i++)
    if (r.also[i] == id) return true;
  return false;
}
