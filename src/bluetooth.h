#pragma once
// Bluetooth Low Energy link to a phone. The board shows up as
// "RiverNode-<id>" and uses the Nordic UART service, a common standard for
// sending lines of text over BLE:
//
//   board  --- TX (notify) --->  phone   messages from phone_messages.cpp
//   board  <--- RX (write) ----  phone   requests, e.g. "refresh"
//
// A message is usually longer than one Bluetooth packet, so it's sent in
// pieces and the phone joins them back up until it gets a '\n'.
//
// With BLUETOOTH set to 0 in config.h (or on a PC build) these do nothing.

////////////////////////
// Function prototypes//
////////////////////////
void bluetooth_begin(const char *node_id);
bool bluetooth_connected(void);
bool bluetooth_just_connected(void);
void bluetooth_send_line(const char *line, int length);
bool bluetooth_take_request(char *request, int size);
////////////////////////
