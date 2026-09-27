#pragma once
// All pins, timing, calibration values and rule thresholds live here.
// Nothing in the rules is hard-coded.
#include <stdint.h>

// ---------------------------------------------------------------- Modes
#ifndef DEMO_MODE
#define DEMO_MODE 1          // 1: 10-min baseline window, 5-min warm-up
#endif
#ifndef SIMULATE
#define SIMULATE 1           // 1: scripted fake readings instead of real sensors
#endif
#define SIM_FAST 1           // SIMULATE only: one "minute" lasts 10 s so the demo runs quickly
#ifndef SIM_REMOTE_NODES
#define SIM_REMOTE_NODES 1   // feed the other network nodes from the simulator
#endif

#define OUTPUT_HUMAN 0
#define OUTPUT_JSON  1
#define OUTPUT_BOTH  2
#define OUTPUT_MODE  OUTPUT_BOTH
#define USE_COLOUR   1       // ANSI colours in the terminal

#define NODE_ID "N1"         // this board's id; must appear in DEFAULT_LAYOUT

// ---------------------------------------------------------------- Pins (ADC1 only: ADC2 is unusable with WiFi)
const uint8_t PIN_PH        = 34;
const uint8_t PIN_TDS       = 35;
const uint8_t PIN_TURBIDITY = 32;
const uint8_t PIN_ONEWIRE   = 4;    // DS18B20 data, 4.7k pull-up to 3.3 V
const uint8_t ADC_SAMPLES   = 16;   // ADC reads averaged per sample

// Voltage dividers: V_sensor = V_adc * ratio. The pH module and TSW-10 are 5 V
// parts; a 10k/10k divider gives 2.0. The STJF TDS board outputs <= 2.3 V.
const float PH_DIVIDER_RATIO   = 2.0f;
const float TDS_DIVIDER_RATIO  = 1.0f;
const float TURB_DIVIDER_RATIO = 2.0f;

// ---------------------------------------------------------------- Timing
const uint32_t SAMPLE_INTERVAL_MS  = 2000;
const uint32_t MINUTE_MS           = (SIMULATE && SIM_FAST) ? 10000 : 60000;
const uint8_t  MAX_SAMPLES_PER_MIN = 40;

// ---------------------------------------------------------------- Calibration / conversion
const float TEMP_FALLBACK_C  = 25.0f;   // used for correction when DS18B20 read fails
const float TEMP_DISCONNECTED_C = -127.0f;
const float TEMP_POWER_ON_C  = 85.0f;
const float TEMP_REF_C       = 25.0f;

const float TDS_K_VALUE      = 1.0f;    // probe cell constant (calibrate with a known solution)
const float TDS_TEMP_COEFF   = 0.02f;   // EC25 = EC / (1 + coeff * (T - 25))
const float TDS_FACTOR       = 0.5f;    // TDS (mg/L) = EC25 * factor

const float PH_V7            = 2.50f;   // module output in pH 7.0 buffer (after divider undo)
const float PH_V4            = 3.05f;   // module output in pH 4.0 buffer

const float TURB_V_CLEAR     = 4.20f;   // TSW-10 output in clear water (measure yours)
const float TURB_V_CURVE_CLEAR = 4.20f; // clear-water voltage the NTU curve was fitted at
const float TURB_V_CURVE_MIN = 2.50f;   // below this the curve is out of range
const float TURB_MAX_NTU     = 3000.0f;

// ---------------------------------------------------------------- Baseline
const uint16_t BASELINE_SLOT_MIN    = DEMO_MODE ? 1 : 5;     // minutes per slot
const uint16_t BASELINE_SLOTS       = DEMO_MODE ? 10 : 288;  // 10 min / 24 h
const uint16_t BASELINE_MIN_MINUTES = DEMO_MODE ? 5 : 60;    // warm-up
const uint8_t  PH_DAY_BUCKETS       = 24;                    // hourly pH min/max
const uint16_t NUTRIENT_MIN_MINUTES = 12 * 60;
const uint16_t DRIFT_SNAPSHOT_MIN   = 6 * 60;
const uint8_t  DRIFT_SNAPSHOTS      = 12;                    // 3 days
const float    DRIFT_PH_LIMIT       = 0.5f;
const float    DRIFT_OTHER_TOL      = 0.10f;
const uint8_t  STEP_WINDOW_MIN      = 30;                    // industrial step-change window

// ---------------------------------------------------------------- Faults
const float    FLATLINE_TOL      = 0.001f;   // 0.1 %
const float    FLATLINE_ABS_TOL  = 0.0001f;  // for values at/near zero
const uint16_t FLATLINE_MIN      = 60;
const float    FLATLINE_NTU_FLOOR = 0.5f;    // clamped clear-water 0 NTU is not "stuck"
const float    RANGE_PH_MIN = 0.0f,   RANGE_PH_MAX = 14.0f;
const float    RANGE_TDS_MIN = 0.0f,  RANGE_TDS_MAX = 5000.0f;
const float    RANGE_NTU_MAX = 3000.0f;
const float    RANGE_TEMP_MIN = -5.0f, RANGE_TEMP_MAX = 50.0f;
const float    TEMP_JUMP_C = 3.0f;

// ---------------------------------------------------------------- Ratio floors (avoid divide-by-zero)
const float BASE_FLOOR_TDS = 1.0f;
const float BASE_FLOOR_NTU = 1.0f;

// ---------------------------------------------------------------- B. Rain filter
const float RAIN_TDS_DROP  = 0.15f;
const float RAIN_NTU_RATIO = 2.0f;
const float RAIN_TEMP_DROP = 0.5f;

// ---------------------------------------------------------------- C. Pollution rules
const float HM_PH_MAX = 6.0f, HM_PH_STRONG = 5.0f, HM_PH_DROP = 1.0f;
const float HM_TDS_RISE = 0.50f, HM_NTU_RATIO = 1.5f;

const float IND_PH_LOW = 6.0f, IND_PH_HIGH = 9.0f;
const float IND_TDS_STEP = 0.30f, IND_TEMP_RISE = 2.0f;

const float ALK_PH_MIN = 9.0f, ALK_PH_STRONG = 10.0f;
const float ALK_TDS_RISE = 0.20f, ALK_NTU_RATIO = 1.5f;

const float SEW_PH_DROP_MIN = 0.3f, SEW_PH_DROP_MAX = 1.0f;
const float SEW_TDS_RISE_MIN = 0.20f, SEW_TDS_RISE_MAX = 0.50f;
const float SEW_NTU_RATIO = 2.0f;
const float SEW_TEMP_RISE_MIN = 0.5f, SEW_TEMP_RISE_MAX = 2.0f;

const float STRONG_BONUS = 0.1f;

// ---------------------------------------------------------------- D. Watch rules
const float NUT_PH_SWING = 1.0f, NUT_PH_HIGH = 9.0f;
const float NUT_TDS_RISE_MIN = 0.10f, NUT_TDS_RISE_MAX = 0.30f;
const float NUT_TEMP_WARM = 25.0f;

const float THERM_TEMP_RISE = 2.0f, THERM_TEMP_ABS = 28.0f;
const float THERM_PH_TOL = 0.3f, THERM_TDS_TOL = 0.10f, THERM_NTU_RATIO = 1.5f;

const float SED_NTU_RATIO = 3.0f, SED_NTU_ABS = 50.0f;
const float SED_TDS_TOL = 0.10f, SED_PH_TOL = 0.3f;

const float SALT_TDS_ABS = 1500.0f, SALT_PH_TOL = 0.3f, SALT_NTU_RATIO = 1.5f;

// ---------------------------------------------------------------- 5. Persistence
const uint8_t PERSIST_ON_MIN  = 3;
const uint8_t PERSIST_OFF_MIN = 5;

// ---------------------------------------------------------------- Network (array of nodes)
const uint8_t  MAX_NODES      = 16;
const uint8_t  NODE_ID_LEN    = 8;
const uint8_t  NODE_PLACE_LEN = 20;
const uint32_t NODE_STALE_MIN = 3;   // node ignored if no report for this long

// simRole: 'U' upstream, 'S' same-position sibling, 'D' downstream, 0 = no simulator
struct NodeLayout { const char* id; uint32_t distanceM; const char* place; bool isLocal; char simRole; };
static const NodeLayout DEFAULT_LAYOUT[] = {
  { "N0",    0, "upstream",    false, 'U' },
  { "N1", 1000, "left bank",   true,  0   },
  { "N2", 1000, "right bank",  false, 'S' },
  { "N3", 2500, "downstream",  false, 'D' },
};
const uint8_t DEFAULT_LAYOUT_COUNT = sizeof(DEFAULT_LAYOUT) / sizeof(DEFAULT_LAYOUT[0]);

// ---------------------------------------------------------------- Display
const uint8_t DISPLAY_WIDTH  = 61;
const uint8_t WRAP_TEXT_COLS = 47;   // text column width after "Possible pollutants: "
