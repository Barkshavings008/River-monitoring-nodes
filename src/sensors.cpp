#include "sensors.h"
#include "config.h"
#include "compensation.h"
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire oneWire(PIN_ONEWIRE);
static DallasTemperature ds18b20(&oneWire);
static SensorVoltages lastVolts = { 0, 0, 0, TEMP_DISCONNECTED_C };

void sensorsBegin() {
    // Default 11 dB attenuation: ~0-3.1 V range.
    analogReadResolution(12);
    ds18b20.begin();
    ds18b20.setResolution(12);
    // Non-blocking: read the result on the next 2 s tick.
    ds18b20.setWaitForConversion(false);
    ds18b20.requestTemperatures();
}

// Averaged, factory-calibrated ADC reading, scaled back up through the
// divider.
static float readVolts(uint8_t pin, float dividerRatio) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; i++) {
        sum += analogReadMilliVolts(pin);
    }
    float averageMv = sum / (float)ADC_SAMPLES;
    return averageMv / 1000.0f * dividerRatio;
}

Reading sensorsRead() {
    Reading r;

    // Result of the previous request.
    float t = ds18b20.getTempCByIndex(0);
    ds18b20.requestTemperatures();
    bool tValid = temperatureValid(t);
    r.v[S_TEMP] = t;
    r.valid[S_TEMP] = tValid;
    float tComp = compensationTemp(t, tValid);

    lastVolts.ph = readVolts(PIN_PH, PH_DIVIDER_RATIO);
    lastVolts.tds = readVolts(PIN_TDS, TDS_DIVIDER_RATIO);
    lastVolts.turbidity = readVolts(PIN_TURBIDITY, TURB_DIVIDER_RATIO);
    lastVolts.tempC = t;

    r.v[S_PH] = phFromVoltage(lastVolts.ph);
    r.v[S_TDS] = tdsFromVoltage(lastVolts.tds, tComp);
    r.v[S_NTU] = ntuFromVoltage(lastVolts.turbidity);
    r.valid[S_PH] = true;
    r.valid[S_TDS] = true;
    r.valid[S_NTU] = true;
    return r;
}

SensorVoltages sensorsLastVoltages() {
    return lastVolts;
}
