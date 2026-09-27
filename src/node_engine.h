#pragma once
// One node's full pipeline: 2 s samples -> 1-min medians -> faults -> rules
// -> persistence -> baseline update -> NodeReport. No Arduino dependency, so
// the same engine runs the real node, simulated nodes, and PC unit tests.
#include "types.h"
#include "config.h"
#include "baseline.h"
#include "rules.h"
#include "persistence.h"

class NodeEngine {
public:
    void begin(const char *id);

    void addSample(const Reading &r);

    // Closes the current minute and fills report. ON/OFF transitions go to
    // events.
    void closeMinute(uint32_t uptimeSec, NodeReport &report,
                     AlertEvent *events, uint8_t maxEvents, uint8_t &nEvents);

    const Baseline &baseline() const {
        return baseline_;
    }

private:
    char id_[NODE_ID_LEN];
    float samples_[S_COUNT][MAX_SAMPLES_PER_MIN];
    uint8_t nSamples_[S_COUNT];
    Baseline baseline_;
    FaultTracker faults_;
    Persistence persist_;
    float tdsHist_[STEP_WINDOW_MIN];   // 1-min TDS values for the step check
    uint8_t tdsHistN_;
    uint8_t tdsHistHead_;

    void buildReport(uint32_t uptimeSec, const RuleInputs &in,
                     const uint8_t fault[S_COUNT], const bool valid[S_COUNT],
                     bool rainPattern, NodeReport &rep) const;
    void sortActiveLabels(LabelId *pollution, uint8_t &nPollution,
                          LabelId *watch, uint8_t &nWatch) const;
};
