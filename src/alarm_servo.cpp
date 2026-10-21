#include "alarm_servo.h"
#include "saved_settings.h"
#include <Arduino.h>
#include <ESP32Servo.h>

Servo alarm_servo;
bool servo_tripped = false;

// Goes straight back to where it was before the board was turned off
void alarm_servo_begin(void) {
#if ALARM_SERVO
    servo_tripped = load_servo_tripped();
    alarm_servo.setPeriodHertz(50); // standard hobby servo
    alarm_servo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
    alarm_servo.write(servo_tripped ? SERVO_ALERT_ANGLE : SERVO_REST_ANGLE);
#endif
}

// Called once a minute with the new report. Once it has turned it stays
// turned, so an alert clearing doesn't move it back.
void alarm_servo_update(struct Node_report report) {
#if ALARM_SERVO
    if (servo_tripped || report.state != ST_ALERT) {
        return;
    }
    servo_tripped = true;
    save_servo_tripped(true);
    alarm_servo.write(SERVO_ALERT_ANGLE);
    Serial.println("Servo: problem detected, turned to alert position (type \"reset\" to put it back)");
#endif
}

// The "reset" command: back to the rest position, ready for the next alert
void alarm_servo_reset(void) {
#if ALARM_SERVO
    servo_tripped = false;
    save_servo_tripped(false);
    alarm_servo.write(SERVO_REST_ANGLE);
#endif
}

bool alarm_servo_tripped(void) {
    return servo_tripped;
}
