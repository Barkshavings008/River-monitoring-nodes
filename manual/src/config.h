#pragma once
// Pins, calibration values and sizes for the helper files.

/////////////////////////
///////// MODES /////////
/////////////////////////

// 1 = the return_...() functions give made-up (but believable) readings, so
//     you can test your logic without the sensors plugged in
// 0 = read the real sensors
#ifndef FAKE_SENSORS
#define FAKE_SENSORS 1
#endif

/////////////////////////
///////// PINS //////////
/////////////////////////

// Only ADC1 pins are used, ADC2 doesn't work while WiFi is on
#define PH_PIN 34
#define TDS_PIN 35
#define TURBIDITY_PIN 32
#define ONEWIRE_PIN 4          // DS18B20 data pin, needs a 4.7k pull-up to 3.3 V
#define ADC_SAMPLES 16         // number of ADC reads averaged for each reading

// Voltage dividers: V_sensor = V_adc * ratio. The pH module and TSW-10 are 5 V
// parts so a 10k/10k divider gives 2.0. The STJF TDS board only outputs <= 2.3 V.
#define PH_DIVIDER_RATIO 2.0f
#define TDS_DIVIDER_RATIO 1.0f
#define TURBIDITY_DIVIDER_RATIO 2.0f

/////////////////////////
////// CALIBRATION //////
/////////////////////////

#define PH_V7 2.50f            // pH module output in pH 7.0 buffer (after undoing the divider)
#define PH_V4 3.05f            // pH module output in pH 4.0 buffer
#define PH_CALIBRATION_TEMP_C 25.0f // water temp the two buffers were measured at

#define TDS_K_VALUE 1.0f       // probe cell constant (calibrate with a known solution)
#define TDS_TEMP_COEFF 0.02f   // EC25 = EC / (1 + coeff * (T - 25))
#define TDS_FACTOR 0.5f        // TDS (mg/L) = EC25 * factor

#define TURB_V_CLEAR 4.20f        // TSW-10 output in clear water (measure your own)
#define TURB_V_CURVE_CLEAR 4.20f  // clear water voltage the NTU curve was made for
#define TURB_V_CURVE_MIN 2.50f    // below this the curve doesn't work anymore
#define TURB_MAX_NTU 3000.0f

#define TEMP_REF_C 25.0f
#define TEMP_FALLBACK_C 25.0f        // used in the pH/TDS maths when the DS18B20 read fails
#define TEMP_DISCONNECTED_C -127.0f  // what the DS18B20 gives when it's unplugged
#define TEMP_POWER_ON_C 85.0f        // what the DS18B20 gives before its first reading

/////////////////////////
////// LINKED LIST //////
/////////////////////////

#define NODE_ID_LEN 8          // longest id is 7 characters
#define NODE_PLACE_LEN 20      // longest place name is 19 characters
