#include "compensation.h"
#include "config.h"
#include <math.h>

bool temperatureValid(float tempC) {
    if (isnan(tempC)) {
        return false;
    }
    if (fabsf(tempC - TEMP_DISCONNECTED_C) < 0.5f) {
        return false;
    }
    if (fabsf(tempC - TEMP_POWER_ON_C) < 0.01f) {
        return false;
    }
    return true;
}

float compensationTemp(float tempC, bool valid) {
    if (valid) {
        return tempC;
    }
    return TEMP_FALLBACK_C;
}

float ecRawFromVoltage(float v) {
    if (v < 0.0f) {
        v = 0.0f;
    }
    return TDS_K_VALUE * (133.42f * v * v * v - 255.86f * v * v + 857.39f * v);
}

float compensateEC(float ecRaw, float tempC) {
    return ecRaw / (1.0f + TDS_TEMP_COEFF * (tempC - TEMP_REF_C));
}

float tdsFromVoltage(float volts, float tempC) {
    float tds = compensateEC(ecRawFromVoltage(volts), tempC) * TDS_FACTOR;
    if (tds < 0.0f) {
        tds = 0.0f;
    }
    return tds;
}

// Straight line through (PH_V7, 7.0) and (PH_V4, 4.0). The spec's
// "(V_pH7 - V)" form has the sign flipped (gives 10.0 at V_pH4).
float phFromVoltage(float volts) {
    return 7.0f + (volts - PH_V7) * 3.0f / (PH_V7 - PH_V4);
}

float ntuFromVoltage(float volts) {
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
