#pragma once
// Turns sensor voltages into real units, and does the temperature correction.
// No Arduino stuff in here so it can be tested on a PC.

////////////////////////
// Function prototypes//
////////////////////////
bool temperature_valid(float temp_c);
float compensation_temp(float temp_c, bool valid);
float ec_raw_from_voltage(float volts);
float compensate_ec(float ec_raw, float temp_c);
float tds_from_voltage(float volts, float temp_c);
float ph_from_voltage(float volts);
float ntu_from_voltage(float volts);
////////////////////////
