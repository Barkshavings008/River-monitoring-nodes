#pragma once
// Stand-in for the Arduino core when the node code is built for the web
// simulator. Serial.print() writes into a text buffer that the page reads,
// so the page shows exactly what the board would print to the serial monitor.
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SERIAL_BUFFER_SIZE 65536

extern char serial_buffer[SERIAL_BUFFER_SIZE];
extern int serial_length;

void serial_add(const char *text);

struct Fake_serial {
    void print(const char *text) {
        serial_add(text);
    }
    void print(char c) {
        char text[2] = { c, '\0' };
        serial_add(text);
    }
    void print(int number) {
        char text[16];
        snprintf(text, 16, "%d", number);
        serial_add(text);
    }
    void print(unsigned long number) {
        char text[24];
        snprintf(text, 24, "%lu", number);
        serial_add(text);
    }
    void println(void) {
        serial_add("\n");
    }
    void println(const char *text) {
        serial_add(text);
        serial_add("\n");
    }
    void println(unsigned long number) {
        print(number);
        serial_add("\n");
    }
};

extern struct Fake_serial Serial;
