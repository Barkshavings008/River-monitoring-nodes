#include "commands.h"
#include "config.h"
#include "display.h"
#include "sensors.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE 80
#define MSG_SIZE 96

namespace {

char line[LINE_SIZE];
uint8_t len = 0;

// Reads a whole number of metres from s into out.
// Returns false if s is empty or is not a number.
bool parseMetres(const char *s, uint32_t &out) {
    if (s == NULL || s[0] == '\0') {
        return false;
    }
    char *end;
    unsigned long v = strtoul(s, &end, 10);
    if (end[0] != '\0') {
        return false;
    }
    out = v;
    return true;
}

// add <id> <metres> [place]
void handleAdd(RiverNetwork &net, uint32_t nowMin) {
    char msg[MSG_SIZE];
    char *id = strtok(NULL, " ");
    char *dist = strtok(NULL, " ");
    char *place = strtok(NULL, "");
    uint32_t m;
    if (id == NULL || !parseMetres(dist, m)) {
        displayMessage("Usage: add <id> <metres> [place]");
        return;
    }

    // Skip leading spaces and quotes, and cut the place at a closing quote.
    if (place != NULL) {
        while (place[0] == ' ' || place[0] == '"') {
            place++;
        }
        char *quote = strchr(place, '"');
        if (quote != NULL) {
            quote[0] = '\0';
        }
    }
    const char *placeName = "added";
    if (place != NULL && place[0] != '\0') {
        placeName = place;
    }

    if (net.addNode(id, m, placeName, false)) {
        snprintf(msg, sizeof(msg), "Added %s at %u m", id, (unsigned)m);
    } else {
        snprintf(msg, sizeof(msg),
                 "Could not add %s (duplicate id or network full)", id);
    }
    displayMessage(msg);
    displayNetworkList(net, nowMin);
}

// del <id>
void handleDel(RiverNetwork &net, uint32_t nowMin) {
    char msg[MSG_SIZE];
    char *id = strtok(NULL, " ");
    int i = net.indexOf(id);
    if (i < 0) {
        displayMessage("Unknown node");
        return;
    }
    if (net.at(i).isLocal) {
        displayMessage("Cannot delete this board's own node");
        return;
    }
    net.removeNode(id);
    snprintf(msg, sizeof(msg), "Deleted %s", id);
    displayMessage(msg);
    displayNetworkList(net, nowMin);
}

// move <id> <metres>
void handleMove(RiverNetwork &net, uint32_t nowMin) {
    char msg[MSG_SIZE];
    char *id = strtok(NULL, " ");
    char *dist = strtok(NULL, " ");
    uint32_t m;
    if (id == NULL || !parseMetres(dist, m)) {
        displayMessage("Usage: move <id> <metres>");
        return;
    }
    if (net.divertNode(id, m)) {
        snprintf(msg, sizeof(msg), "Moved %s to %u m", id, (unsigned)m);
    } else {
        snprintf(msg, sizeof(msg), "Unknown node %s", id);
    }
    displayMessage(msg);
    displayNetworkList(net, nowMin);
}

// Runs one command line.
void handle(char *cmd, RiverNetwork &net, uint32_t nowMin) {
    char *verb = strtok(cmd, " ");
    if (verb == NULL) {
        return;
    }

    if (strcmp(verb, "list") == 0) {
        displayNetworkList(net, nowMin);
    } else if (strcmp(verb, "add") == 0) {
        handleAdd(net, nowMin);
    } else if (strcmp(verb, "del") == 0) {
        handleDel(net, nowMin);
    } else if (strcmp(verb, "move") == 0) {
        handleMove(net, nowMin);
    } else if (strcmp(verb, "cal") == 0) {
        if (SIMULATE) {
            displayMessage("cal: not available in SIMULATE mode");
        } else {
            displayVoltages(sensorsLastVoltages());
        }
    } else {
        displayHelp();
    }
}

}  // namespace

void commandsPoll(RiverNetwork &net, uint32_t nowMin) {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            line[len] = '\0';
            handle(line, net, nowMin);
            len = 0;
        } else if (c != '\r' && len < sizeof(line) - 1) {
            line[len] = c;
            len++;
        }
    }
}
