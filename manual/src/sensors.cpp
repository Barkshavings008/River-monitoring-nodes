#include "sensors.h"
#include "config.h"
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire one_wire(ONEWIRE_PIN);
DallasTemperature temp_sensor(&one_wire); // DS18B20

// Clean river water, used when FAKE_SENSORS is 1 (change with set_fake_readings())
struct Water_reading fake_water = { 7.2f, 210.0f, 8.0f, 17.0f, true };

////////////////////////
// Function prototypes//
////////////////////////
float read_volts(int pin, float divider_ratio);
float temp_for_maths(float temp_c);
float fake_noise(float amount);
////////////////////////

void wake_up_sensors(void) {
    analogReadResolution(12); // default 11 dB attenuation reads about 0 - 3.1 V
    temp_sensor.begin();
    temp_sensor.setResolution(12); // 0.0625 C steps, takes about 0.75 s per reading
    randomSeed(analogRead(TURBIDITY_PIN)); // only used by the fake readings
}

/////////////////////////
////// ONE READING //////
/////////////////////////

float return_water_temp(void) {
    if (FAKE_SENSORS) {
        return fake_water.water_temp + fake_noise(0.05f);
    }
    temp_sensor.requestTemperatures(); // waits until the reading is done
    return temp_sensor.getTempCByIndex(0);
}

// pH, corrected for the water temperature
float return_ph(void) {
    if (FAKE_SENSORS) {
        return fake_water.ph + fake_noise(0.03f);
    }
    float temp = temp_for_maths(return_water_temp());
    return ph_from_voltage(return_ph_voltage(), temp);
}

// TDS in mg/L, corrected to 25 C using the water temperature
float return_tds(void) {
    if (FAKE_SENSORS) {
        return fake_water.tds * (1.0f + fake_noise(0.015f));
    }
    float temp = temp_for_maths(return_water_temp());
    return tds_from_voltage(return_tds_voltage(), temp);
}

// Turbidity in NTU (doesn't need the temperature)
float return_turbidity(void) {
    if (FAKE_SENSORS) {
        return fake_water.turbidity * (1.0f + fake_noise(0.03f));
    }
    return turbidity_from_voltage(return_turbidity_voltage());
}

struct Water_reading read_all_sensors(void) {
    struct Water_reading reading;

    if (FAKE_SENSORS) {
        reading.water_temp = fake_water.water_temp + fake_noise(0.05f);
        reading.temp_ok = true;
        reading.ph = fake_water.ph + fake_noise(0.03f);
        reading.tds = fake_water.tds * (1.0f + fake_noise(0.015f));
        reading.turbidity = fake_water.turbidity * (1.0f + fake_noise(0.03f));
        return reading;
    }

    reading.water_temp = return_water_temp(); // only read the slow temp sensor once
    reading.temp_ok = water_temp_ok(reading.water_temp);
    float temp = temp_for_maths(reading.water_temp);

    reading.ph = ph_from_voltage(return_ph_voltage(), temp);
    reading.tds = tds_from_voltage(return_tds_voltage(), temp);
    reading.turbidity = turbidity_from_voltage(return_turbidity_voltage());
    return reading;
}

// The DS18B20 gives -127 C when it's unplugged and 85 C before its first reading
bool water_temp_ok(float temp_c) {
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

// Changes what the fake readings are centred on, e.g. set_fake_readings(4.6, 612, 18, 17.3)
// to pretend there's an acid spill. Does nothing when FAKE_SENSORS is 0.
void set_fake_readings(float ph, float tds, float turbidity, float water_temp) {
    fake_water.ph = ph;
    fake_water.tds = tds;
    fake_water.turbidity = turbidity;
    fake_water.water_temp = water_temp;
}

/////////////////////////
//////// VOLTAGES ///////
/////////////////////////

float return_ph_voltage(void) {
    return read_volts(PH_PIN, PH_DIVIDER_RATIO);
}

float return_tds_voltage(void) {
    return read_volts(TDS_PIN, TDS_DIVIDER_RATIO);
}

float return_turbidity_voltage(void) {
    return read_volts(TURBIDITY_PIN, TURBIDITY_DIVIDER_RATIO);
}

/////////////////////////
////////// MATHS ////////
/////////////////////////

// Straight line through (PH_V7, pH 7) and (PH_V4, pH 4), measured at
// PH_CALIBRATION_TEMP_C. The probe's voltage per pH step grows with
// temperature (in kelvin), so the slope gets scaled to the water temp.
float ph_from_voltage(float volts, float temp_c) {
    float slope = 3.0f / (PH_V7 - PH_V4);   // pH per volt at the calibration temp
    float temp_scale = (PH_CALIBRATION_TEMP_C + 273.15f) / (temp_c + 273.15f);
    return 7.0f + (volts - PH_V7) * slope * temp_scale;
}

// STJF TDS Meter V1.0 (same curve as the DFRobot one), corrected to 25 C
float tds_from_voltage(float volts, float temp_c) {
    if (volts < 0.0f) {
        volts = 0.0f;
    }
    float v = volts;
    float ec = TDS_K_VALUE * (133.42f * v * v * v - 255.86f * v * v + 857.39f * v); // uS/cm
    float ec25 = ec / (1.0f + TDS_TEMP_COEFF * (temp_c - TEMP_REF_C));
    float tds = ec25 * TDS_FACTOR;
    if (tds < 0.0f) {
        tds = 0.0f;
    }
    return tds;
}

// TS-300B: scale to the clear water voltage, use the curve, then keep it
// between 0 and TURB_MAX_NTU
float turbidity_from_voltage(float volts) {
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

/////////////////////////
// Function definitions//
/////////////////////////

// Averages a few factory-calibrated ADC reads, then scales back up through
// the voltage divider
float read_volts(int pin, float divider_ratio) {
    unsigned long sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        sum += analogReadMilliVolts(pin);
    }
    float average_mv = sum / (float)ADC_SAMPLES;
    return average_mv / 1000.0f * divider_ratio;
}

// Temperature to use in the pH/TDS maths (25 C if the sensor failed)
float temp_for_maths(float temp_c) {
    if (water_temp_ok(temp_c)) {
        return temp_c;
    } else {
        return TEMP_FALLBACK_C;
    }
}

// Random number between -amount and +amount (makes the fake readings wobble)
float fake_noise(float amount) {
    return amount * (random(-1000, 1001) / 1000.0f);
}
