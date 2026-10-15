#pragma once
// Copy this file to src/secrets.h and fill it in. secrets.h is in .gitignore,
// so your WiFi password and ntfy topic never get uploaded to GitHub.
//
// The ESP32 only joins 2.4 GHz networks without a login page. An iPhone
// hotspot works: Settings > Personal Hotspot > turn on "Maximise
// Compatibility".

#define WIFI_SSID_1 "your hotspot name"
#define WIFI_PASSWORD_1 "your hotspot password"

// Optional second network (leave as "" if you don't need it)
#define WIFI_SSID_2 ""
#define WIFI_PASSWORD_2 ""

// The topic you subscribed to in the ntfy app. Anyone who knows it can read
// it, so make it hard to guess.
#define NTFY_TOPIC "rivernode-change-me"
