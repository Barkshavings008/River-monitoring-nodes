#pragma once
// Real sensor hardware: pH module, STJF TDS Meter V1.0, TSW-10 turbidity,
// DS18B20.
#include "types.h"

// Voltages at the sensor outputs (divider already undone).
struct SensorVoltages {
    float ph;
    float tds;
    float turbidity;
    float tempC;
};

// Sets up the ADC and the temperature sensor.
void sensorsBegin();

// One 2 s sample, converted to units and temperature corrected.
Reading sensorsRead();

// Last raw voltages, for calibration (serial command "cal").
SensorVoltages sensorsLastVoltages();
