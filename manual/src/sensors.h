#pragma once
// Turns the raw sensor signals into real units, so you just call return_ph()
// and get the pH. The water temperature is read and used automatically in the
// pH and TDS maths.
//
// Sensors: Logo-Rnaenaor V2.0 pH module, STJF TDS Meter V1.0, TS-300B turbidity,
// DS18B20 water temp.

// All four readings taken together
struct Water_reading {
    float ph;
    float tds;          // total dissolved solids, mg/L (same as ppm)
    float turbidity;    // NTU (0 = crystal clear)
    float water_temp;   // degrees C
    bool temp_ok;       // false = the temp sensor failed and 25 C was used in the maths
};

////////////////////////
// Function prototypes//
////////////////////////

// Call once in setup() before reading anything
void wake_up_sensors(void);

// Each one takes a fresh reading. return_ph() and return_tds() read the water
// temp themselves, so each call takes about 0.75 s (the DS18B20 is slow).
float return_ph(void);
float return_tds(void);
float return_turbidity(void);
float return_water_temp(void);  // gives -127 if the temp sensor is unplugged

// Reads everything in one go (only reads the temp once, so it's the fastest
// way to get all four)
struct Water_reading read_all_sensors(void);

// true if the temperature is a real reading (not -127 or the 85 power-on value)
bool water_temp_ok(float temp_c);

// FAKE_SENSORS only: changes what the fake readings are centred on, e.g.
// set_fake_readings(4.6, 612, 18, 17.3) to pretend there's an acid spill
void set_fake_readings(float ph, float tds, float turbidity, float water_temp);

// Raw sensor voltages, for calibrating (put the results in config.h)
float return_ph_voltage(void);
float return_tds_voltage(void);
float return_turbidity_voltage(void);

// The maths on its own, if you already have the voltage and temperature
float ph_from_voltage(float volts, float temp_c);
float tds_from_voltage(float volts, float temp_c);
float turbidity_from_voltage(float volts);
////////////////////////
