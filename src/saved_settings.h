#pragma once
// Settings that are remembered when the board is turned off (stored in the
// ESP32's flash). On a PC build they're only kept until the program ends.

////////////////////////
// Function prototypes//
////////////////////////
bool load_armed(void);
void save_armed(bool armed);
bool load_servo_tripped(void);
void save_servo_tripped(bool tripped);
////////////////////////
