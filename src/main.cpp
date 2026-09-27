#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "network.h"
#include "node_engine.h"
#include "sensors.h"
#include "sim.h"
#include "display.h"
#include "commands.h"

#define MAX_EVENTS 8

static RiverNetwork network;
static NodeEngine localEngine;
#if SIM_REMOTE_NODES
static SimRemotes simRemotes;
#endif

static uint32_t lastSampleMs = 0;
static uint32_t lastMinuteMs = 0;
static uint32_t minuteIndex = 0;      // completed minutes since boot
static uint32_t sampleInMinute = 0;

// Runs once at power on: builds the network and starts the sensors.
void setup() {
    Serial.begin(115200);
    delay(200);

    for (uint8_t i = 0; i < DEFAULT_LAYOUT_COUNT; i++) {
        const NodeLayout &l = DEFAULT_LAYOUT[i];
        network.addNode(l.id, l.distanceM, l.place, l.isLocal);
    }
    localEngine.begin(NODE_ID);

#if !SIMULATE
    sensorsBegin();
#endif
#if SIM_REMOTE_NODES
    simRemotes.begin();
#endif

    displayBanner(network);
    lastSampleMs = millis();
    lastMinuteMs = lastSampleMs;
}

// Runs forever: takes a sample every 2 s and closes a minute every minute.
void loop() {
    commandsPoll(network, minuteIndex);
    uint32_t now = millis();

    if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
        lastSampleMs += SAMPLE_INTERVAL_MS;
#if SIMULATE
        Reading r = simReading(SIM_LOCAL, minuteIndex, sampleInMinute);
#else
        Reading r = sensorsRead();
#endif
        sampleInMinute++;
        localEngine.addSample(r);
    }

    if (now - lastMinuteMs >= MINUTE_MS) {
        lastMinuteMs += MINUTE_MS;
        uint32_t uptimeSec = now / 1000;

        NodeReport report;
        AlertEvent events[MAX_EVENTS];
        uint8_t nEvents = 0;
        localEngine.closeMinute(uptimeSec, report, events, MAX_EVENTS,
                                nEvents);

#if SIM_REMOTE_NODES
        simRemotes.tick(minuteIndex, uptimeSec, network, minuteIndex + 1);
#endif
        minuteIndex++;
        sampleInMinute = 0;
        network.updateReport(NODE_ID, report, minuteIndex);

        displayEvents(events, nEvents);
        NetworkAssessment a = network.assess(NODE_ID, minuteIndex);
        displayMinute(report, a, network, minuteIndex);
    }
}
