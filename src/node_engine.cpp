#include "node_engine.h"
#include "labels.h"
#include <string.h>

namespace {

// Inserts id into list[0..n), keeping the list sorted by confidence
// (highest first), and adds one to n.
void insertByConf(const Persistence &persist, LabelId *list, uint8_t &n,
                  LabelId id) {
    uint8_t j = n;
    n++;
    while (j > 0 && persist.conf(list[j - 1]) < persist.conf(id)) {
        list[j] = list[j - 1];
        j--;
    }
    list[j] = id;
}

}  // namespace

void NodeEngine::begin(const char *id) {
    strncpy(id_, id, NODE_ID_LEN - 1);
    id_[NODE_ID_LEN - 1] = '\0';
    memset(nSamples_, 0, sizeof(nSamples_));
    baseline_.reset();
    faults_.reset();
    persist_.reset();
    tdsHistN_ = 0;
    tdsHistHead_ = 0;
}

// Stores one sample. Invalid readings and samples past the limit are
// dropped.
void NodeEngine::addSample(const Reading &r) {
    for (uint8_t s = 0; s < S_COUNT; s++) {
        if (r.valid[s] && nSamples_[s] < MAX_SAMPLES_PER_MIN) {
            samples_[s][nSamples_[s]] = r.v[s];
            nSamples_[s]++;
        }
    }
}

void NodeEngine::closeMinute(uint32_t uptimeSec, NodeReport &rep,
                             AlertEvent *events, uint8_t maxEvents,
                             uint8_t &nEvents) {
    nEvents = 0;

    // 1-minute medians (median ignores one-off spikes).
    float now[S_COUNT];
    bool valid[S_COUNT];
    for (uint8_t s = 0; s < S_COUNT; s++) {
        if (nSamples_[s] > 0) {
            valid[s] = true;
            now[s] = medianOf(samples_[s], nSamples_[s]);
        } else {
            valid[s] = false;
            now[s] = 0.0f;
        }
        nSamples_[s] = 0;
    }

    uint8_t fault[S_COUNT];
    checkFaults(faults_, now, valid, fault);

    RuleInputs in;
    for (uint8_t s = 0; s < S_COUNT; s++) {
        in.now[s] = now[s];
        in.base[s] = baseline_.value(s);
        in.ok[s] = valid[s] && fault[s] == F_NONE && baseline_.has(s);
    }

    // Oldest TDS value in the step-change window.
    if (tdsHistN_ == 0) {
        in.hasTdsStepRef = false;
        in.tdsStepRef = 0.0f;
    } else if (tdsHistN_ < STEP_WINDOW_MIN) {
        in.hasTdsStepRef = true;
        in.tdsStepRef = tdsHist_[0];
    } else {
        in.hasTdsStepRef = true;
        in.tdsStepRef = tdsHist_[tdsHistHead_];
    }

    in.phDayMin = 0.0f;
    in.phDayMax = 0.0f;
    if (DEMO_MODE) {
        in.hasDayRange = false;
    } else {
        in.hasDayRange = baseline_.phDayRange(in.phDayMin, in.phDayMax);
    }

    if (valid[S_TDS] && fault[S_TDS] == F_NONE) {
        tdsHist_[tdsHistHead_] = now[S_TDS];
        tdsHistHead_ = (tdsHistHead_ + 1) % STEP_WINDOW_MIN;
        if (tdsHistN_ < STEP_WINDOW_MIN) {
            tdsHistN_++;
        }
    }

    RuleOutput out;
    out.n = 0;
    out.rain = false;
    if (baseline_.ready()) {
        out = evaluateRules(in);
        nEvents = persist_.update(out.hits, out.n, events, maxEvents);
    }

    // Keep pollution (and rain) out of "normal": freeze while any pattern is
    // seen or any label is active; faulted sensors are never added.
    bool frozen = out.n > 0 || persist_.anyActive();
    bool add[S_COUNT];
    for (uint8_t s = 0; s < S_COUNT; s++) {
        add[s] = valid[s] && fault[s] == F_NONE && !frozen;
    }

    buildReport(uptimeSec, in, fault, valid, out.rain, rep);
    baseline_.addMinute(now, valid, add);
}

// Splits the active labels into pollution and watch lists, each sorted by
// confidence (highest first).
void NodeEngine::sortActiveLabels(LabelId *pollution, uint8_t &nPollution,
                                  LabelId *watch, uint8_t &nWatch) const {
    nPollution = 0;
    nWatch = 0;
    for (uint8_t i = LBL_NONE + 1; i < LBL_RAIN; i++) {
        LabelId id = (LabelId)i;
        if (persist_.active(id)) {
            if (labelInfo(id).category == CAT_POLLUTION) {
                insertByConf(persist_, pollution, nPollution, id);
            } else {
                insertByConf(persist_, watch, nWatch, id);
            }
        }
    }
}

void NodeEngine::buildReport(uint32_t uptimeSec, const RuleInputs &in,
                             const uint8_t fault[S_COUNT],
                             const bool valid[S_COUNT], bool rainPattern,
                             NodeReport &rep) const {
    memset(&rep, 0, sizeof(rep));
    strncpy(rep.id, id_, sizeof(rep.id) - 1);
    rep.t = uptimeSec;
    bool anyFault = baseline_.phDriftDetected();
    for (uint8_t s = 0; s < S_COUNT; s++) {
        rep.now[s] = in.now[s];
        rep.nowValid[s] = valid[s];
        rep.base[s] = in.base[s];
        rep.baseValid[s] = baseline_.has(s);
        rep.fault[s] = fault[s];
        if (fault[s] != F_NONE) {
            anyFault = true;
        }
    }
    rep.phRecalibrate = baseline_.phDriftDetected();
    rep.learnedMin = baseline_.learnedMinutes();
    rep.learnNeeded = BASELINE_MIN_MINUTES;
    rep.rainPattern = rainPattern;

    if (!baseline_.ready()) {
        rep.state = ST_BASELINE_BUILDING;
        rep.label = LBL_NONE;
        return;
    }

    // Active labels: c = pollution, d = watch.
    LabelId c[4];
    LabelId d[4];
    uint8_t nC = 0;
    uint8_t nD = 0;
    sortActiveLabels(c, nC, d, nD);
    bool rain = persist_.active(LBL_RAIN);

    LabelId also[MAX_ALSO];
    uint8_t nAlso = 0;
    if (nC > 0) {
        rep.state = ST_ALERT;
        rep.label = c[0];
        for (uint8_t i = 1; i < nC; i++) {
            also[nAlso] = c[i];
            nAlso++;
        }
        for (uint8_t i = 0; i < nD; i++) {
            also[nAlso] = d[i];
            nAlso++;
        }
    } else if (anyFault) {
        rep.state = ST_FAULT;
        rep.label = LBL_FAULT;
        for (uint8_t i = 0; i < nD; i++) {
            also[nAlso] = d[i];
            nAlso++;
        }
    } else if (nD > 0) {
        rep.state = ST_WATCH;
        rep.label = d[0];
        for (uint8_t i = 1; i < nD; i++) {
            also[nAlso] = d[i];
            nAlso++;
        }
    } else {
        rep.state = ST_NORMAL;
        if (rain) {
            rep.label = LBL_RAIN;
        } else {
            rep.label = LBL_NONE;
        }
    }
    if (rain && rep.label != LBL_RAIN) {
        also[nAlso] = LBL_RAIN;
        nAlso++;
    }

    if (rep.label == LBL_NONE || rep.label == LBL_FAULT) {
        rep.conf = 0.0f;
    } else {
        rep.conf = persist_.conf(rep.label);
    }
    memcpy(rep.also, also, nAlso * sizeof(LabelId));
    rep.nAlso = nAlso;
}
