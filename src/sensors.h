#pragma once
// Real sensor hardware: pH module, STJF TDS Meter V1.0, TSW-10 turbidity, DS18B20.
#include "types.h"

struct SensorVoltages {
  float ph, tds, turbidity;   // at the sensor output (divider already undone)
  float tempC;
};

void sensorsBegin();

// One 2 s sample, converted to units and temperature corrected.
Reading sensorsRead();

// Last raw voltages, for calibration (serial command "cal").
SensorVoltages sensorsLastVoltages();
