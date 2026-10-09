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
#define TDS_PIN 33
#define TURBIDITY_PIN 32
#define ONEWIRE_PIN 4        // DS18B20 data pin, needs a 4.7k pull-up to 3.3 V
#define ADC_SAMPLES 16       // number of ADC reads averaged for each sample

// Voltage dividers: V_sensor = V_adc * ratio. The pH module (Logo-Rnaenaor V2.0)
// and the TS-300B turbidity sensor are 5 V parts, so each goes through
//   sensor output -> 12k -> ESP32 pin -> 10k -> GND
// The pin sees 10 / (12 + 10) of the sensor voltage, so ratio = 22 / 10 = 2.2
// (the TS-300B's 4.5 V maximum reaches the pin as 2.05 V).
// The STJF TDS board only outputs <= 2.3 V, so it's wired straight in.
#define PH_DIVIDER_RATIO 2.2f
#define TDS_DIVIDER_RATIO 1.0f
#define TURBIDITY_DIVIDER_RATIO 2.2f

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

// Logo-Rnaenaor V2.0 pH module: the output goes in a straight line with pH, so
// it's calibrated from two buffers. Use the "cal" command with the probe in
// each buffer and copy the pH voltage here (it's already scaled back up
// through the divider). The defaults are typical for this kind of 5 V module.
#define PH_V7 2.50f            // module output in pH 7.0 buffer
#define PH_V4 3.05f            // module output in pH 4.0 buffer

// TS-300B turbidity sensor: 0-4.5 V output, the voltage goes DOWN as the water
// gets cloudier. It uses the standard curve for this sensor family (made for
// a 4.2 V clear-water reading), scaled to your sensor's own clear-water
// voltage. Rated 0-1000 NTU, so readings above that are only rough.
#define TURB_V_CLEAR 4.50f        // TS-300B output in clear water (measure your own with "cal")
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
// After a pattern or alert ends, keep "normal" frozen this long, so an alert
// that keeps switching on and off doesn't slowly teach the node that the
// polluted water is normal
#if DEMO_MODE
#define BASELINE_HOLD_MIN 10
#else
#define BASELINE_HOLD_MIN (3 * 60)
#endif

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
// The STJF TDS board can't output more than 2.3 V (about 1120 mg/L at 25 C,
// more in cold water once it's corrected to 25 C). These are voltages so they
// work at any water temperature:
#define TDS_V_MAXED 2.2f            // at or above this the sensor is pinned at its top (very
                                    // salty water), so a steady reading isn't "stuck"
#define TDS_V_IMPOSSIBLE 2.6f       // the board can't output this much: wiring fault
#define RANGE_NTU_MAX 3000.0f
#define RANGE_TEMP_MIN -5.0f
#define RANGE_TEMP_MAX 50.0f
// A jump of more than TEMP_JUMP_C in one minute is only a fault if it comes
// straight back the next minute (within TEMP_RETURN_C of where it started).
// A jump that stays could be real, e.g. a warm outflow reaching the probe.
#define TEMP_JUMP_C 3.0f
#define TEMP_RETURN_C 1.0f

// Smallest baseline we divide by (stops dividing by zero)
#define BASE_FLOOR_TDS 1.0f
// The TS-300B curve is very steep near clear water (4.2 V is about 1 NTU but
// 4.1 V is about 360 NTU), so normal ADC wobble is tens of NTU. Turbidity
// ratios use at least this as "normal", and a turbidity rise only counts if
// it's also more than NTU_MIN_RISE above normal. These are starting values:
// leave the sensor in still tap water for 30 minutes and see how much the
// minute readings move, then set both to a bit more than that. Higher is
// safer against false alerts but misses diluted sewage (a sewage overflow
// mixed into a stream may only add 30 NTU).
#define BASE_FLOOR_NTU 20.0f
#define NTU_MIN_RISE 25.0f

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
#define SEW_TDS_RISE_MIN 0.20f      // no upper limit: a big spill into a low-salt river still counts
#define SEW_NTU_RATIO 2.0f
#define SEW_TEMP_RISE_MIN 0.5f
#define SEW_TEMP_RISE_MAX 2.0f

#define STRONG_BONUS 0.1f

/////////////////////////
//////// D. WATCH ///////
/////////////////////////

#define NUT_PH_SWING 1.0f
#define NUT_PH_HIGH 9.0f
#define NUT_TDS_RISE_MIN 0.10f      // optional: fertiliser nitrate barely moves TDS
#define NUT_TDS_RISE_MAX 0.30f
#define NUT_TEMP_WARM 25.0f

// "Normal" is a 24 h median, not the time of day, and shallow streams warm
// 2-4 C on a sunny afternoon, so the rise has to be more than that
#define THERM_TEMP_RISE 3.0f
#define THERM_TEMP_ABS 30.0f        // Sydney summer rivers can reach 28 C on their own
#define THERM_PH_TOL 0.3f
#define THERM_TDS_TOL 0.10f
#define THERM_NTU_RATIO 1.5f

#define SED_NTU_RATIO 3.0f
#define SED_NTU_ABS 50.0f
#define SED_TDS_TOL 0.10f
#define SED_PH_TOL 0.3f

// Seawater pushing upstream. The TDS sensor tops out at about 1120 mg/L, so
// the limit has to be below that. Seawater is pH ~8.1, so mixing with it can
// raise a river's pH by up to about 1.
#define SALT_TDS_ABS 900.0f
#define SALT_PH_RISE_MAX 1.0f
#define SALT_PH_DROP_MAX 0.3f
#define SALT_NTU_RATIO 1.5f

// Treated wastewater / fertiliser runoff: clear water with extra dissolved
// salts and a slightly lower pH
// (0.3 pH and 20 % TDS, so cheap probe drift doesn't set it off)
#define EFF_TDS_RISE 0.20f
#define EFF_PH_DROP_MIN 0.3f
#define EFF_PH_DROP_MAX 1.0f
#define EFF_NTU_RATIO 1.5f      // turbidity has to stay below this (muddy = sewage rule instead)
#define EFF_TEMP_RISE_MIN 0.3f
#define EFF_TEMP_RISE_MAX 3.0f

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
#define MAX_BRANCHES 8      // separate streams / rivers in the network
#define BRANCH_NAME_LEN 12
#define MAIN_BRANCH "main"  // branch used when a node is added without naming one

/////////////////////////
////// WATER SPEED //////
/////////////////////////

// The water speed is worked out from pollution moving down the river: when
// the same kind of pollution shows up at an upstream node and then at a node
// further down, speed = river distance between them / time between alerts.
#define SPEED_QUIET_MIN 60        // a node only counts if it had no alerts for this long first
#define SPEED_PAIR_WINDOW_MIN 360 // the upstream alert has to be from the last 6 hours
                                  // (and no rain in between, since rain pauses and restarts alerts)
#define SPEED_MAX_MPS 5.0f        // faster than this can't be a real river, so it's ignored
#define MAX_DETECTIONS 32         // pollution arrivals remembered for pairing

/////////////////////////
/////// BLUETOOTH ///////
/////////////////////////

// Sends the readings and alerts to a phone over Bluetooth Low Energy (see
// bluetooth.cpp, and docs/phone/index.html for the phone page)
#ifndef BLUETOOTH
#define BLUETOOTH 1
#endif
#define BLE_NAME_PREFIX "RiverNode-" // the board shows up as RiverNode-N1
#define BLE_LINE_SIZE 4096           // biggest message sent to the phone (a full network is about 3.9 KB)

/////////////////////////
//////// DISPLAY ////////
/////////////////////////

#define DISPLAY_WIDTH 61
#define WRAP_TEXT_COLS 47   // text width after "Possible pollutants: "
