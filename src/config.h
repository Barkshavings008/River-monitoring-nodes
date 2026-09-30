#pragma once
// All pins, timing, calibration values and rule thresholds live here.
// Nothing in the rules is hard-coded.

/////////////////////////
///////// MODES /////////
/////////////////////////

#ifndef DEMO_MODE
#define DEMO_MODE 1          // 1 = 10 min baseline window, 5 min warm-up (quick testing)
#endif
#ifndef SIMULATE
#define SIMULATE 1           // 1 = scripted fake readings instead of the real sensors
#endif
#define SIM_FAST 1           // SIMULATE only: one "minute" only lasts 10 s so the demo runs quickly
#ifndef SIM_REMOTE_NODES
#define SIM_REMOTE_NODES 1   // 1 = the other river nodes are faked by the simulator
#endif

#define OUTPUT_HUMAN 0
#define OUTPUT_JSON 1
#define OUTPUT_BOTH 2
#define OUTPUT_MODE OUTPUT_BOTH
#define USE_COLOUR 1         // coloured text in the serial monitor

#define NODE_ID "N1"         // this board's id (see setup_river_layout() in main.cpp)

/////////////////////////
///////// PINS //////////
/////////////////////////

// Only ADC1 pins are used, ADC2 doesn't work while WiFi is on
#define PH_PIN 34
#define TDS_PIN 35
#define TURBIDITY_PIN 32
#define ONEWIRE_PIN 4        // DS18B20 data pin, needs a 4.7k pull-up to 3.3 V
#define ADC_SAMPLES 16       // number of ADC reads averaged for each sample

// Voltage dividers: V_sensor = V_adc * ratio. The pH module and TSW-10 are 5 V
// parts so a 10k/10k divider gives 2.0. The STJF TDS board only outputs <= 2.3 V.
#define PH_DIVIDER_RATIO 2.0f
#define TDS_DIVIDER_RATIO 1.0f
#define TURBIDITY_DIVIDER_RATIO 2.0f

/////////////////////////
///////// TIMING ////////
/////////////////////////

#define SAMPLE_INTERVAL_MS 2000
#if SIMULATE && SIM_FAST
#define MINUTE_MS 10000
#else
#define MINUTE_MS 60000
#endif
#define MAX_SAMPLES_PER_MIN 40

/////////////////////////
////// CALIBRATION //////
/////////////////////////

#define TEMP_FALLBACK_C 25.0f        // used for the TDS correction when the DS18B20 read fails
#define TEMP_DISCONNECTED_C -127.0f  // what the DS18B20 returns when it's unplugged
#define TEMP_POWER_ON_C 85.0f        // what the DS18B20 returns before its first reading
#define TEMP_REF_C 25.0f

#define TDS_K_VALUE 1.0f       // probe cell constant (calibrate with a known solution)
#define TDS_TEMP_COEFF 0.02f   // EC25 = EC / (1 + coeff * (T - 25))
#define TDS_FACTOR 0.5f        // TDS (mg/L) = EC25 * factor

#define PH_V7 2.50f            // module output in pH 7.0 buffer (after undoing the divider)
#define PH_V4 3.05f            // module output in pH 4.0 buffer

#define TURB_V_CLEAR 4.20f        // TSW-10 output in clear water (measure your own)
#define TURB_V_CURVE_CLEAR 4.20f  // clear water voltage the NTU curve was made for
#define TURB_V_CURVE_MIN 2.50f    // below this the curve doesn't work anymore
#define TURB_MAX_NTU 3000.0f

/////////////////////////
//////// BASELINE ///////
/////////////////////////

#if DEMO_MODE
#define BASELINE_SLOT_MIN 1      // minutes in each slot
#define BASELINE_SLOTS 10        // 10 min
#define BASELINE_MIN_MINUTES 5   // warm-up
#else
#define BASELINE_SLOT_MIN 5      // minutes in each slot
#define BASELINE_SLOTS 288       // 24 h
#define BASELINE_MIN_MINUTES 60  // warm-up
#endif
#define PH_DAY_BUCKETS 24               // hourly pH min/max
#define NUTRIENT_MIN_MINUTES (12 * 60)
#define DRIFT_SNAPSHOT_MIN (6 * 60)
#define DRIFT_SNAPSHOTS 12              // 3 days
#define DRIFT_PH_LIMIT 0.5f
#define DRIFT_OTHER_TOL 0.10f
#define STEP_WINDOW_MIN 30              // window for the industrial step change

/////////////////////////
///////// FAULTS ////////
/////////////////////////

#define FLATLINE_TOL 0.001f         // 0.1 %
#define FLATLINE_ABS_TOL 0.0001f    // for values at or near zero
#define FLATLINE_MIN 60
#define FLATLINE_NTU_FLOOR 0.5f     // clear water sitting at 0 NTU isn't "stuck"
#define RANGE_PH_MIN 0.0f
#define RANGE_PH_MAX 14.0f
#define RANGE_TDS_MIN 0.0f
#define RANGE_TDS_MAX 5000.0f
#define RANGE_NTU_MAX 3000.0f
#define RANGE_TEMP_MIN -5.0f
#define RANGE_TEMP_MAX 50.0f
#define TEMP_JUMP_C 3.0f

// Smallest baseline we divide by (stops dividing by zero)
#define BASE_FLOOR_TDS 1.0f
#define BASE_FLOOR_NTU 1.0f

/////////////////////////
///// B. RAIN FILTER ////
/////////////////////////

#define RAIN_TDS_DROP 0.15f
#define RAIN_NTU_RATIO 2.0f
#define RAIN_TEMP_DROP 0.5f

/////////////////////////
////// C. POLLUTION /////
/////////////////////////

#define HM_PH_MAX 6.0f
#define HM_PH_STRONG 5.0f
#define HM_PH_DROP 1.0f
#define HM_TDS_RISE 0.50f
#define HM_NTU_RATIO 1.5f

#define IND_PH_LOW 6.0f
#define IND_PH_HIGH 9.0f
#define IND_TDS_STEP 0.30f
#define IND_TEMP_RISE 2.0f

#define ALK_PH_MIN 9.0f
#define ALK_PH_STRONG 10.0f
#define ALK_TDS_RISE 0.20f
#define ALK_NTU_RATIO 1.5f

#define SEW_PH_DROP_MIN 0.3f
#define SEW_PH_DROP_MAX 1.0f
#define SEW_TDS_RISE_MIN 0.20f
#define SEW_TDS_RISE_MAX 0.50f
#define SEW_NTU_RATIO 2.0f
#define SEW_TEMP_RISE_MIN 0.5f
#define SEW_TEMP_RISE_MAX 2.0f

#define STRONG_BONUS 0.1f

/////////////////////////
//////// D. WATCH ///////
/////////////////////////

#define NUT_PH_SWING 1.0f
#define NUT_PH_HIGH 9.0f
#define NUT_TDS_RISE_MIN 0.10f
#define NUT_TDS_RISE_MAX 0.30f
#define NUT_TEMP_WARM 25.0f

#define THERM_TEMP_RISE 2.0f
#define THERM_TEMP_ABS 28.0f
#define THERM_PH_TOL 0.3f
#define THERM_TDS_TOL 0.10f
#define THERM_NTU_RATIO 1.5f

#define SED_NTU_RATIO 3.0f
#define SED_NTU_ABS 50.0f
#define SED_TDS_TOL 0.10f
#define SED_PH_TOL 0.3f

#define SALT_TDS_ABS 1500.0f
#define SALT_PH_TOL 0.3f
#define SALT_NTU_RATIO 1.5f

/////////////////////////
///// 5. PERSISTENCE ////
/////////////////////////

#define PERSIST_ON_MIN 3    // minutes in a row before a label turns on
#define PERSIST_OFF_MIN 5   // minutes in a row before a label turns off

/////////////////////////
//////// NETWORK ////////
/////////////////////////

#define MAX_NODES 16
#define NODE_ID_LEN 8
#define NODE_PLACE_LEN 20
#define NODE_STALE_MIN 3    // a node is ignored if it hasn't reported for this long

/////////////////////////
//////// DISPLAY ////////
/////////////////////////

#define DISPLAY_WIDTH 61
#define WRAP_TEXT_COLS 47   // text width after "Possible pollutants: "
