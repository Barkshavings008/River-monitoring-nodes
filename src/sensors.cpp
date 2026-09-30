#include "sensors.h"
#include "compensation.h"
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire one_wire(ONEWIRE_PIN);
DallasTemperature temp_sensor(&one_wire); // DS18B20
struct Sensor_voltages last_volts = { 0, 0, 0, TEMP_DISCONNECTED_C };

////////////////////////
// Function prototypes//
////////////////////////
float read_volts(int pin, float divider_ratio);
////////////////////////

void wake_up_sensors(void) {
    analogReadResolution(12); // default 11 dB attenuation reads about 0 - 3.1 V
    temp_sensor.begin();
    temp_sensor.setResolution(12);
    temp_sensor.setWaitForConversion(false); // don't wait, read the result on the next 2 s tick
    temp_sensor.requestTemperatures();
}

// One 2 s sample, turned into real units and temperature corrected
struct Reading read_sensors(void) {
    struct Reading reading;

    float temp = temp_sensor.getTempCByIndex(0); // result of the last request
    temp_sensor.requestTemperatures();
    bool temp_valid = temperature_valid(temp);
    reading.value[S_TEMP] = temp;
    reading.valid[S_TEMP] = temp_valid;
    float correction_temp = compensation_temp(temp, temp_valid);

    last_volts.ph = read_volts(PH_PIN, PH_DIVIDER_RATIO);
    last_volts.tds = read_volts(TDS_PIN, TDS_DIVIDER_RATIO);
    last_volts.turbidity = read_volts(TURBIDITY_PIN, TURBIDITY_DIVIDER_RATIO);
    last_volts.temp_c = temp;

    reading.value[S_PH] = ph_from_voltage(last_volts.ph);
    reading.value[S_TDS] = tds_from_voltage(last_volts.tds, correction_temp);
    reading.value[S_NTU] = ntu_from_voltage(last_volts.turbidity);
    reading.valid[S_PH] = true;
    reading.valid[S_TDS] = true;
    reading.valid[S_NTU] = true;
    return reading;
}

// Last raw voltages, for calibrating (the "cal" serial command)
struct Sensor_voltages last_sensor_voltages(void) {
    return last_volts;
}

/////////////////////////
// Function definitions//
/////////////////////////

// Averages a few factory-calibrated ADC reads, then scales back up through
// the voltage divider
float read_volts(int pin, float divider_ratio) {
    uint32_t sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        sum += analogReadMilliVolts(pin);
    }
    float average_mv = sum / (float)ADC_SAMPLES;
    return average_mv / 1000.0f * divider_ratio;
}
