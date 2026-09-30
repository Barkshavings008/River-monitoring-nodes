#pragma once
// The real sensors: pH module, STJF TDS Meter V1.0, TSW-10 turbidity and DS18B20.
#include "types.h"

// Voltages at the sensor outputs (divider already undone), for calibrating
struct Sensor_voltages {
    float ph;
    float tds;
    float turbidity;
    float temp_c;
};

////////////////////////
// Function prototypes//
////////////////////////
void wake_up_sensors(void);
struct Reading read_sensors(void);
struct Sensor_voltages last_sensor_voltages(void);
////////////////////////
