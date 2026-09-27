#include "commands.h"
#include "config.h"
#include "display.h"
#include "sensors.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

namespace {

char line[80];
uint8_t len = 0;

bool parseMetres(const char* s, uint32_t& out) {
  if (!s || !*s) return false;
  char* end;
  unsigned long v = strtoul(s, &end, 10);
  if (*end) return false;
  out = v;
  return true;
}

void handle(char* cmd, RiverNetwork& net, uint32_t nowMin) {
  char msg[96];
  char* verb = strtok(cmd, " ");
  if (!verb) return;

  if (!strcmp(verb, "list")) {
    displayNetworkList(net, nowMin);
  } else if (!strcmp(verb, "add")) {
    char* id = strtok(NULL, " ");
    char* dist = strtok(NULL, " ");
    char* place = strtok(NULL, "");
    uint32_t m;
    if (!id || !parseMetres(dist, m)) { displayMessage("Usage: add <id> <metres> [place]"); return; }
    if (place) while (*place == ' ' || *place == '"') place++;
    if (place) { char* q = strchr(place, '"'); if (q) *q = '\0'; }
    if (net.addNode(id, m, place && *place ? place : "added", false))
      snprintf(msg, sizeof(msg), "Added %s at %u m", id, (unsigned)m);
    else
      snprintf(msg, sizeof(msg), "Could not add %s (duplicate id or network full)", id);
    displayMessage(msg);
    displayNetworkList(net, nowMin);
  } else if (!strcmp(verb, "del")) {
    char* id = strtok(NULL, " ");
    int i = net.indexOf(id);
    if (i < 0) { displayMessage("Unknown node"); return; }
    if (net.at(i).isLocal) { displayMessage("Cannot delete this board's own node"); return; }
    net.removeNode(id);
    snprintf(msg, sizeof(msg), "Deleted %s", id);
    displayMessage(msg);
    displayNetworkList(net, nowMin);
  } else if (!strcmp(verb, "move")) {
    char* id = strtok(NULL, " ");
    char* dist = strtok(NULL, " ");
    uint32_t m;
    if (!id || !parseMetres(dist, m)) { displayMessage("Usage: move <id> <metres>"); return; }
    if (net.divertNode(id, m)) snprintf(msg, sizeof(msg), "Moved %s to %u m", id, (unsigned)m);
    else snprintf(msg, sizeof(msg), "Unknown node %s", id);
    displayMessage(msg);
    displayNetworkList(net, nowMin);
  } else if (!strcmp(verb, "cal")) {
    if (SIMULATE) displayMessage("cal: not available in SIMULATE mode");
    else displayVoltages(sensorsLastVoltages());
  } else {
    displayHelp();
  }
}

}  // namespace

void commandsPoll(RiverNetwork& net, uint32_t nowMin) {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line[len] = '\0';
      handle(line, net, nowMin);
      len = 0;
    } else if (len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }
}
