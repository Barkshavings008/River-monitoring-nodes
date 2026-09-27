#include "display.h"
#include "config.h"
#include "labels.h"
#include <Arduino.h>
#include <string.h>

namespace {

const char* const C_RED     = "\033[31m";
const char* const C_GREEN   = "\033[32m";
const char* const C_YELLOW  = "\033[33m";
const char* const C_MAGENTA = "\033[35m";
const char* const C_GREY    = "\033[90m";
const char* const C_RESET   = "\033[0m";

const char* col(const char* c) { return USE_COLOUR ? c : ""; }
const char* reset() { return USE_COLOUR ? C_RESET : ""; }

const char* categoryColour(Category c) {
  switch (c) {
    case CAT_POLLUTION: return col(C_RED);
    case CAT_WATCH:     return col(C_YELLOW);
    case CAT_FILTER:    return col(C_GREY);
    case CAT_FAULT:     return col(C_MAGENTA);
    default:            return col(C_GREEN);
  }
}

const char* stateColour(NodeState s) {
  switch (s) {
    case ST_ALERT:  return col(C_RED);
    case ST_WATCH:  return col(C_YELLOW);
    case ST_FAULT:  return col(C_MAGENTA);
    case ST_NORMAL: return col(C_GREEN);
    default:        return col(C_GREY);
  }
}

const char* categoryTag(Category c) {
  switch (c) {
    case CAT_POLLUTION: return "[POLLUTION]";
    case CAT_WATCH:     return "[WATCH]";
    case CAT_FILTER:    return "[FILTER]";
    case CAT_FAULT:     return "[FAULT]";
    default:            return "";
  }
}

const char* faultCodeStr(uint8_t f) {
  switch (f) {
    case F_NO_READING:   return "NO_READING";
    case F_OUT_OF_RANGE: return "OUT_OF_RANGE";
    case F_FLATLINE:     return "FLATLINE";
    case F_TEMP_JUMP:    return "JUMP";
    default:             return "NONE";
  }
}

const char* faultReason(uint8_t sensor, uint8_t f) {
  switch (f) {
    case F_NO_READING:
      return sensor == S_TEMP ? "no reading (DS18B20 gave -127/85 C); using 25 C for correction"
                              : "no reading";
    case F_OUT_OF_RANGE: return "reading out of range - check probe, wiring or calibration";
    case F_FLATLINE:     return "reading stuck for 60+ min - check probe and wiring";
    case F_TEMP_JUMP:    return "jumped more than 3 C in 1 min - check probe";
    default:             return "";
  }
}

void repeat(char ch, uint8_t n) { while (n--) Serial.print(ch); }

// Prints lead, then text word-wrapped; continuation lines are indented to
// line up under the first word of text.
void printWrapped(const char* lead, const char* text, size_t width = WRAP_TEXT_COLS) {
  size_t indent = strlen(lead);
  size_t colPos = 0;
  Serial.print(lead);
  const char* p = text;
  while (*p) {
    while (*p == ' ') p++;
    const char* w = p;
    while (*p && *p != ' ') p++;
    size_t len = p - w;
    if (!len) break;
    if (colPos > 0 && colPos + 1 + len > width) {
      Serial.println();
      repeat(' ', indent);
      colPos = 0;
    }
    if (colPos > 0) { Serial.print(' '); colPos++; }
    Serial.write((const uint8_t*)w, len);
    colPos += len;
  }
  Serial.println();
}

void formatValue(char* buf, size_t n, uint8_t s, float v, bool valid) {
  if (!valid) { snprintf(buf, n, "--"); return; }
  switch (s) {
    case S_PH:  snprintf(buf, n, "%.2f", v); break;
    case S_TDS: snprintf(buf, n, "%.0f mg/L", v); break;
    case S_NTU: snprintf(buf, n, "%.1f NTU", v); break;
    default:    snprintf(buf, n, "%.1f C", v); break;
  }
}

void formatChange(char* buf, size_t n, uint8_t s, float now, float base, bool valid) {
  if (!valid) { snprintf(buf, n, "--"); return; }
  switch (s) {
    case S_PH: snprintf(buf, n, "%+.2f", now - base); break;
    case S_TDS: {
      float b = base < BASE_FLOOR_TDS ? BASE_FLOOR_TDS : base;
      snprintf(buf, n, "%+.0f%%", (now - b) / b * 100.0f);
      break;
    }
    case S_NTU: {
      float b = base < BASE_FLOOR_NTU ? BASE_FLOOR_NTU : base;
      snprintf(buf, n, "x%.1f", now / b);
      break;
    }
    default: snprintf(buf, n, "%+.1f C", now - base); break;
  }
}

void labelCode(const NodeReport& r, char* buf, size_t n) {
  if (r.label != LBL_FAULT) { snprintf(buf, n, "%s", labelInfo(r.label).code); return; }
  for (uint8_t s = 0; s < S_COUNT; s++) {
    if (r.fault[s] != F_NONE) {
      snprintf(buf, n, "FAULT_%s_%s", sensorCode(s), faultCodeStr(r.fault[s]));
      return;
    }
  }
  snprintf(buf, n, r.phRecalibrate ? "PH_RECALIBRATE" : "FAULT");
}

const char* findingCode(NetFinding f) {
  switch (f) {
    case NF_NO_PEERS:             return "NO_PEERS";
    case NF_SOURCE_LOCAL_SIDE:    return "SOURCE_LOCAL_SIDE";
    case NF_SOURCE_CROSS_SECTION: return "SOURCE_CROSS_SECTION";
    case NF_SOURCE_BETWEEN:       return "SOURCE_BETWEEN";
    case NF_FROM_UPSTREAM:        return "FROM_UPSTREAM";
    case NF_RAIN_CONFIRMED:       return "RAIN_CONFIRMED";
    case NF_RAIN_UNCONFIRMED:     return "RAIN_UNCONFIRMED";
    default:                      return "NONE";
  }
}

void findingText(const NetworkAssessment& a, char* buf, size_t n) {
  unsigned up = (unsigned)a.upstreamDist, here = (unsigned)a.localDist;
  switch (a.finding) {
    case NF_NO_PEERS:
      snprintf(buf, n, "No fresh data from other nodes - cannot compare.");
      break;
    case NF_SOURCE_LOCAL_SIDE:
      if (a.hasUpstream)
        snprintf(buf, n, "Source likely between %s (%u m) and %s (%u m), on %s's side - "
                 "other node(s) at this position are normal.", a.upstreamId, up, a.localId, here, a.localId);
      else
        snprintf(buf, n, "Source likely on %s's side at %u m - other node(s) here are normal.",
                 a.localId, here);
      break;
    case NF_SOURCE_CROSS_SECTION:
      if (a.hasUpstream)
        snprintf(buf, n, "Whole river width affected at %u m; source likely between %s (%u m) and here.",
                 here, a.upstreamId, up);
      else
        snprintf(buf, n, "Whole river width affected at %u m.", here);
      break;
    case NF_SOURCE_BETWEEN:
      snprintf(buf, n, "Source likely between %s (%u m) and %s (%u m) - upstream is normal.",
               a.upstreamId, up, a.localId, here);
      break;
    case NF_FROM_UPSTREAM:
      snprintf(buf, n, "Same pattern upstream at %s (%u m) - pollution is coming from further upstream.",
               a.upstreamId, up);
      break;
    case NF_RAIN_CONFIRMED:
      snprintf(buf, n, "Rain pattern at %u of %u nodes - confirmed weather event.",
               (unsigned)a.rainPeers + 1, (unsigned)a.peers + 1);
      break;
    case NF_RAIN_UNCONFIRMED:
      snprintf(buf, n, "Rain pattern at this node only (%u of %u) - possible stormwater outfall or leak nearby.",
               (unsigned)a.rainPeers + 1, (unsigned)a.peers + 1);
      break;
    default:
      buf[0] = '\0';
  }
}

void printHeader(const NodeReport& r) {
  unsigned long t = r.t;
  char mid[64];
  snprintf(mid, sizeof(mid), " NODE %s | %02lu:%02lu:%02lu | %s ",
           r.id, t / 3600, (t / 60) % 60, t % 60, stateName(r.state));
  uint8_t len = strlen(mid);
  uint8_t left = 16;
  uint8_t right = DISPLAY_WIDTH > left + len + 3 ? DISPLAY_WIDTH - left - len : 3;
  Serial.print(stateColour(r.state));
  repeat('=', left);
  Serial.print(mid);
  repeat('=', right);
  Serial.println(reset());
}

void printReadings(const NodeReport& r) {
  Serial.printf("%-12s%-10s%-10s%s\n", "Reading", "Now", "Normal", "Change");
  for (uint8_t s = 0; s < S_COUNT; s++) {
    char now[16], base[16], change[16];
    formatValue(now, sizeof(now), s, r.now[s], r.nowValid[s]);
    formatValue(base, sizeof(base), s, r.base[s], r.baseValid[s]);
    formatChange(change, sizeof(change), s, r.now[s], r.base[s], r.nowValid[s] && r.baseValid[s]);
    Serial.printf("%-12s%-10s%-10s%s\n", sensorName(s), now, base, change);
  }
}

void printFaults(const NodeReport& r) {
  for (uint8_t s = 0; s < S_COUNT; s++) {
    if (r.fault[s] == F_NONE) continue;
    Serial.printf("%s[FAULT] %s: %s%s\n", col(C_MAGENTA), sensorName(s), faultReason(s, r.fault[s]), reset());
  }
  if (r.phRecalibrate)
    Serial.printf("%s[FAULT] pH: baseline drifted > %.1f over 3 days - PH_RECALIBRATE%s\n",
                  col(C_MAGENTA), DRIFT_PH_LIMIT, reset());
}

void printLabel(LabelId id, float conf) {
  const LabelInfo& li = labelInfo(id);
  Serial.printf("%s%s %s%s", categoryColour(li.category), categoryTag(li.category), li.name, reset());
  if (li.category == CAT_POLLUTION || li.category == CAT_WATCH) Serial.printf("  (conf %.2f)", conf);
  Serial.println();
  printWrapped("  Possible pollutants: ", li.pollutants);
  printWrapped("  What it means:       ", li.meaning);
}

void printAlso(const NodeReport& r) {
  if (!r.nAlso) return;
  char buf[160] = "";
  for (uint8_t i = 0; i < r.nAlso; i++) {
    if (i) strncat(buf, ", ", sizeof(buf) - strlen(buf) - 1);
    strncat(buf, labelInfo(r.also[i]).code, sizeof(buf) - strlen(buf) - 1);
  }
  printWrapped("  Also flagged:        ", buf);
}

void printDisclaimer() {
  Serial.printf("  Note: %s\n        %s\n", DISCLAIMER_LINE1, DISCLAIMER_LINE2);
}

void printNetwork(const NodeReport& r, const NetworkAssessment& a, const RiverNetwork& net, uint32_t nowMin) {
  int li = net.indexOf(r.id);
  uint32_t localDist = li >= 0 ? net.at(li).distanceM : 0;
  Serial.println("Network (upstream -> downstream):");
  for (uint8_t i = 0; i < net.count(); i++) {
    const RiverNode& n = net.at(i);
    char status[48];
    if (!n.hasReport) snprintf(status, sizeof(status), "no data");
    else if (!n.isLocal && !net.isFresh(n, nowMin)) snprintf(status, sizeof(status), "offline");
    else if (n.report.label == LBL_NONE) snprintf(status, sizeof(status), "%s", stateName(n.report.state));
    else {
      char code[40];
      labelCode(n.report, code, sizeof(code));
      snprintf(status, sizeof(status), "%s %s", stateName(n.report.state), code);
    }
    const char* tag = n.isLocal ? "  <- this node" : (n.distanceM == localDist ? "  <- same position" : "");
    const char* c = n.hasReport && net.isFresh(n, nowMin) ? stateColour(n.report.state) : col(C_GREY);
    Serial.printf("  %-4s %6um  %-12s %s%s%s%s\n", n.id, (unsigned)n.distanceM, n.place,
                  c, status, reset(), tag);
  }
  char text[160];
  findingText(a, text, sizeof(text));
  if (text[0]) {
    const char* c = a.finding == NF_RAIN_UNCONFIRMED ? col(C_YELLOW)
                  : a.finding == NF_RAIN_CONFIRMED   ? col(C_GREY)
                  : a.finding == NF_NO_PEERS         ? "" : col(C_RED);
    Serial.print(c);
    printWrapped("  >> ", text, DISPLAY_WIDTH - 5);
    Serial.print(reset());
  }
}

void printHuman(const NodeReport& r, const NetworkAssessment& a, const RiverNetwork& net, uint32_t nowMin) {
  printFaults(r);
  printHeader(r);
  printReadings(r);

  if (r.state == ST_BASELINE_BUILDING) {
    Serial.printf("Learning normal levels: %u/%u min\n", (unsigned)r.learnedMin, (unsigned)r.learnNeeded);
  } else if (r.state == ST_NORMAL && r.label == LBL_NONE) {
    Serial.printf("Status: %snormal%s\n", col(C_GREEN), reset());
  } else {
    repeat('-', DISPLAY_WIDTH);
    Serial.println();
    if (r.state == ST_FAULT) {
      Serial.printf("%sStatus: sensor fault (see [FAULT] lines above)%s\n", col(C_MAGENTA), reset());
    } else {
      printLabel(r.label, r.conf);
      if (r.state == ST_NORMAL) Serial.println("Status: normal (filtered, no alert)");
    }
    printAlso(r);
    bool warn = r.state == ST_ALERT || r.state == ST_WATCH;
    for (uint8_t i = 0; i < r.nAlso && !warn; i++) warn = labelInfo(r.also[i]).category == CAT_WATCH;
    if (warn) printDisclaimer();
  }
  repeat('=', DISPLAY_WIDTH);
  Serial.println();
  printNetwork(r, a, net, nowMin);
  Serial.println();
}

void jsonNum(float v, bool valid, uint8_t decimals) {
  if (!valid) { Serial.print("null"); return; }
  Serial.printf("%.*f", decimals, v);
}

void jsonValues(const float* v, const bool* valid) {
  Serial.print("\"ph\":");   jsonNum(v[S_PH], valid[S_PH], 2);
  Serial.print(",\"tds\":"); jsonNum(v[S_TDS], valid[S_TDS], 0);
  Serial.print(",\"ntu\":"); jsonNum(v[S_NTU], valid[S_NTU], 1);
  Serial.print(",\"temp\":"); jsonNum(v[S_TEMP], valid[S_TEMP], 1);
}

void printJson(const NodeReport& r, const NetworkAssessment& a) {
  char code[40];
  labelCode(r, code, sizeof(code));
  Serial.printf("JSON:{\"node\":\"%s\",\"pos\":%u,\"t\":%u,\"state\":\"%s\",\"label\":\"%s\",\"conf\":%.2f,",
                r.id, (unsigned)a.localDist, (unsigned)r.t, stateName(r.state), code, r.conf);
  if (r.nAlso) {
    Serial.print("\"also\":[");
    for (uint8_t i = 0; i < r.nAlso; i++) Serial.printf("%s\"%s\"", i ? "," : "", labelInfo(r.also[i]).code);
    Serial.print("],");
  }
  jsonValues(r.now, r.nowValid);
  Serial.print(",\"base\":{");
  jsonValues(r.base, r.baseValid);
  Serial.print("},\"faults\":[");
  bool first = true;
  for (uint8_t s = 0; s < S_COUNT; s++) {
    if (r.fault[s] == F_NONE) continue;
    Serial.printf("%s\"%s_%s\"", first ? "" : ",", sensorCode(s), faultCodeStr(r.fault[s]));
    first = false;
  }
  if (r.phRecalibrate) Serial.printf("%s\"PH_RECALIBRATE\"", first ? "" : ",");
  Serial.printf("],\"net\":{\"finding\":\"%s\",\"upstream\":", findingCode(a.finding));
  if (a.hasUpstream) Serial.printf("\"%s\"", a.upstreamId);
  else Serial.print("null");
  Serial.printf(",\"siblings\":%u,\"siblings_alerting\":%u,\"peers\":%u,\"rain_peers\":%u}}\n",
                (unsigned)a.siblings, (unsigned)a.siblingsAlerting, (unsigned)a.peers, (unsigned)a.rainPeers);
}

}  // namespace

void displayBanner(const RiverNetwork& net) {
  Serial.println();
  repeat('=', DISPLAY_WIDTH);
  Serial.println();
  Serial.printf("River water-quality node %s\n", NODE_ID);
  Serial.printf("Mode: %s%s | minute = %u s | baseline %u min, warm-up %u min\n",
                SIMULATE ? "SIMULATE" : "SENSORS", DEMO_MODE ? " + DEMO" : "",
                (unsigned)(MINUTE_MS / 1000), (unsigned)(BASELINE_SLOTS * BASELINE_SLOT_MIN),
                (unsigned)BASELINE_MIN_MINUTES);
  repeat('=', DISPLAY_WIDTH);
  Serial.println();
  displayNetworkList(net, 0);
  displayHelp();
}

void displayHelp() {
  Serial.println("Commands: list | add <id> <metres> [place] | del <id> | move <id> <metres> | cal | help");
}

void displayEvents(const AlertEvent* ev, uint8_t n) {
  if (OUTPUT_MODE == OUTPUT_JSON) return;
  for (uint8_t i = 0; i < n; i++) {
    const LabelInfo& li = labelInfo(ev[i].id);
    if (li.category != CAT_POLLUTION && li.category != CAT_WATCH) continue;
    if (ev[i].on)
      Serial.printf("%s>>> ALERT ON: %s (conf %.2f)%s\n", categoryColour(li.category), li.shortName, ev[i].conf, reset());
    else
      Serial.printf("%s>>> ALERT OFF: %s%s\n", col(C_GREEN), li.shortName, reset());
  }
}

void displayMinute(const NodeReport& r, const NetworkAssessment& a, const RiverNetwork& net, uint32_t nowMin) {
  if (OUTPUT_MODE != OUTPUT_JSON) printHuman(r, a, net, nowMin);
  if (OUTPUT_MODE != OUTPUT_HUMAN) printJson(r, a);
}

void displayNetworkList(const RiverNetwork& net, uint32_t nowMin) {
  Serial.printf("Network: %u node(s) at %u position(s), capacity %u\n",
                (unsigned)net.count(), (unsigned)net.positionCount(), (unsigned)MAX_NODES);
  for (uint8_t p = 0; p < net.positionCount(); p++) {
    uint32_t d = net.positionDistance(p);
    uint8_t idx[MAX_NODES];
    uint8_t n = net.nodesAt(d, idx, MAX_NODES);
    Serial.printf("  Position %u @ %u m:", (unsigned)(p + 1), (unsigned)d);
    for (uint8_t k = 0; k < n; k++) {
      const RiverNode& node = net.at(idx[k]);
      const char* status = node.isLocal ? "this node"
                         : !node.hasReport ? "no data"
                         : net.isFresh(node, nowMin) ? stateName(node.report.state) : "offline";
      Serial.printf("%s %s (%s, %s)", k ? "," : "", node.id, node.place, status);
    }
    Serial.println();
  }
}

void displayMessage(const char* msg) {
  Serial.println(msg);
}

void displayVoltages(const SensorVoltages& v) {
  Serial.printf("Calibration voltages: pH %.3f V | TDS %.3f V | turbidity %.3f V | temp %.2f C\n",
                v.ph, v.tds, v.turbidity, v.tempC);
  Serial.println("  Set PH_V7 / PH_V4 (in buffers) and TURB_V_CLEAR (clear water) in config.h");
}
