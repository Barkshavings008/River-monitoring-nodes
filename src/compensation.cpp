#include "compensation.h"
#include "config.h"
#include <math.h>

// The DS18B20 gives -127 C when it's unplugged and 85 C before its first reading
bool temperature_valid(float temp_c) {
    if (isnan(temp_c)) {
        return false;
    }
    if (fabsf(temp_c - TEMP_DISCONNECTED_C) < 0.5f) {
        return false;
    }
    if (fabsf(temp_c - TEMP_POWER_ON_C) < 0.01f) {
        return false;
    }
    return true;
}

// Temperature to use for the TDS correction (25 C if the reading is bad)
float compensation_temp(float temp_c, bool valid) {
    if (valid) {
        return temp_c;
    } else {
        return TEMP_FALLBACK_C;
    }
}

// STJF TDS Meter V1.0 (same curve as the DFRobot one). Gives uS/cm at the
// water's temperature.
float ec_raw_from_voltage(float v) {
    if (v < 0.0f) {
        v = 0.0f;
    }
    return TDS_K_VALUE * (133.42f * v * v * v - 255.86f * v * v + 857.39f * v);
}

// EC25 = EC / (1 + 0.02 * (T - 25))
float compensate_ec(float ec_raw, float temp_c) {
    return ec_raw / (1.0f + TDS_TEMP_COEFF * (temp_c - TEMP_REF_C));
}

// TDS in mg/L, corrected to 25 C
float tds_from_voltage(float volts, float temp_c) {
    float tds = compensate_ec(ec_raw_from_voltage(volts), temp_c) * TDS_FACTOR;
    if (tds < 0.0f) {
        tds = 0.0f;
    }
    return tds;
}

// Straight line through (PH_V7, 7.0) and (PH_V4, 4.0). The spec's
// "(V_pH7 - V)" version has the sign flipped (it gives 10.0 at V_pH4).
float ph_from_voltage(float volts) {
    return 7.0f + (volts - PH_V7) * 3.0f / (PH_V7 - PH_V4);
}

// TSW-10 turbidity: scale to the clear water voltage, use the curve, then
// keep it between 0 and TURB_MAX_NTU
float ntu_from_voltage(float volts) {
    float v = volts * TURB_V_CURVE_CLEAR / TURB_V_CLEAR;
    if (v < TURB_V_CURVE_MIN) {
        return TURB_MAX_NTU;
    }
    float ntu = -1120.4f * v * v + 5742.3f * v - 4352.9f;
    if (ntu < 0.0f) {
        ntu = 0.0f;
    }
    if (ntu > TURB_MAX_NTU) {
        ntu = TURB_MAX_NTU;
    }
    return ntu;
}
