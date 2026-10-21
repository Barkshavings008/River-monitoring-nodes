#pragma once
// A servo that swings to SERVO_ALERT_ANGLE degrees the first time a pollution
// alert comes on (e.g. to raise a flag) and stays there, even after the alert
// clears or the board is turned off. The "reset" command puts it back.
#include "types.h"

////////////////////////
// Function prototypes//
////////////////////////
void alarm_servo_begin(void);
void alarm_servo_update(struct Node_report report);
void alarm_servo_reset(void);
bool alarm_servo_tripped(void);
////////////////////////
