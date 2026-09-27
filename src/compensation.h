#pragma once
// Voltage -> unit conversion and temperature correction.
// Pure functions (no Arduino) so they can be unit tested on a PC.

// DS18B20 returns -127 C when disconnected and 85 C before its first
// conversion.
bool temperatureValid(float tempC);

// Temperature to use for correction: the reading, or TEMP_FALLBACK_C if
// invalid.
float compensationTemp(float tempC, bool valid);

// STJF TDS Meter V1.0 (DFRobot-compatible curve). Returns uS/cm at probe
// temperature.
float ecRawFromVoltage(float volts);

// EC25 = EC / (1 + 0.02 * (T - 25))
float compensateEC(float ecRaw, float tempC);

// TDS in mg/L, corrected to 25 C.
float tdsFromVoltage(float volts, float tempC);

// Two-point calibration against the pH 7 and pH 4 buffer voltages in config.
float phFromVoltage(float volts);

// TSW-10 turbidity: normalise to the clear-water voltage, apply the curve,
// clamp >= 0.
float ntuFromVoltage(float volts);
