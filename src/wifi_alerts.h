#pragma once
// Sends phone notifications over WiFi through ntfy (ntfy.sh). Each
// notification is a small JSON message (built in phone_messages.cpp) that is
// posted to ntfy, and the ntfy app on the phone pops it up.
//
// Sending happens in a separate background task, so a slow or missing WiFi
// connection never holds up the readings. Notifications wait in a queue
// until WiFi connects, and each one is tried a few times before giving up.
//
// With WIFI_ALERTS set to 0, no WiFi name in secrets.h, or on a PC build,
// these do nothing.

struct Wifi_status {
    bool enabled;          // WiFi name and ntfy topic are set
    bool connected;
    char network[33];      // which WiFi network it's on
    char ip[16];
    int sent;              // notifications ntfy accepted
    int failed;            // notifications given up on
    int waiting;           // notifications in the queue
    int last_code;         // last answer from ntfy (200 = OK, below 0 = couldn't reach it)
    unsigned long free_memory;
};

////////////////////////
// Function prototypes//
////////////////////////
void wifi_alerts_begin(void);
bool wifi_alerts_enabled(void);
bool wifi_alerts_connected(void);
bool wifi_alerts_send(const char *json);
struct Wifi_status wifi_alerts_status(void);
////////////////////////
