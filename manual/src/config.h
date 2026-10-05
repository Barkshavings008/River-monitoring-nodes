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
#define TDS_PIN 33
#define TURBIDITY_PIN 32
#define ONEWIRE_PIN 4          // DS18B20 data pin, needs a 4.7k pull-up to 3.3 V
#define ADC_SAMPLES 16         // number of ADC reads averaged for each reading

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
////// CALIBRATION //////
/////////////////////////

// Logo-Rnaenaor V2.0 pH module: the output goes in a straight line with pH, so
// it's calibrated from two buffers. Use the "cal" command with the probe in
// each buffer and copy the pH voltage here (it's already scaled back up
// through the divider). The defaults are typical for this kind of 5 V module.
#define PH_V7 2.50f            // module output in pH 7.0 buffer
#define PH_V4 3.05f            // module output in pH 4.0 buffer
#define PH_CALIBRATION_TEMP_C 25.0f // water temp the two buffers were measured at

#define TDS_K_VALUE 1.0f       // probe cell constant (calibrate with a known solution)
#define TDS_TEMP_COEFF 0.02f   // EC25 = EC / (1 + coeff * (T - 25))
#define TDS_FACTOR 0.5f        // TDS (mg/L) = EC25 * factor

// TS-300B turbidity sensor: 0-4.5 V output, the voltage goes DOWN as the water
// gets cloudier. It uses the standard curve for this sensor family (made for
// a 4.2 V clear-water reading), scaled to your sensor's own clear-water
// voltage. Rated 0-1000 NTU, so readings above that are only rough.
#define TURB_V_CLEAR 4.50f        // TS-300B output in clear water (measure your own with "cal")
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
