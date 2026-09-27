#pragma once
// River network stored as a fixed array of nodes, kept sorted by distance
// downstream. Nodes with the same distance share a "position" (e.g. the two
// banks at one point) and are compared with each other.
//
//   index:   0        1          2           3
//   node:    N0       N1         N2          N3
//   dist:    0 m      1000 m     1000 m      2500 m
//            [pos 0]  [------ pos 1 ------]  [pos 2]
//
// No heap allocation: add/remove/divert shift array elements.
#include "types.h"
#include "config.h"

struct RiverNode {
    char id[NODE_ID_LEN];
    char place[NODE_PLACE_LEN];
    uint32_t distanceM;          // distance downstream from the reference point
    bool isLocal;                // this board
    bool hasReport;
    uint32_t lastUpdateMin;
    NodeReport report;
};

enum NetFinding : uint8_t {
    NF_NONE,                 // nothing to compare / nothing flagged
    NF_NO_PEERS,             // flagged, but no fresh data from other nodes
    NF_SOURCE_LOCAL_SIDE,    // alert here, same-position siblings normal
    NF_SOURCE_CROSS_SECTION, // alert here and at every sibling
    NF_SOURCE_BETWEEN,       // alert here, nearest upstream position normal
    NF_FROM_UPSTREAM,        // same label at the nearest upstream position
    NF_RAIN_CONFIRMED,       // rain pattern at most nodes
    NF_RAIN_UNCONFIRMED      // rain pattern only here: possible outfall / leak
};

struct NetworkAssessment {
    NetFinding finding;
    char localId[NODE_ID_LEN];
    uint32_t localDist;
    bool hasUpstream;
    char upstreamId[NODE_ID_LEN];
    uint32_t upstreamDist;
    uint8_t siblings;            // fresh nodes at the same position
                                 // (excluding local)
    uint8_t siblingsAlerting;
    uint8_t peers;               // fresh nodes anywhere (excluding local)
    uint8_t rainPeers;
};

class RiverNetwork {
public:
    RiverNetwork() : count_(0) {
    }

    void clear() {
        count_ = 0;
    }

    // Inserts in distance order (after existing nodes at the same distance).
    bool addNode(const char *id, uint32_t distanceM, const char *place,
                 bool isLocal = false);
    bool removeNode(const char *id);
    // Moves a node to a new distance, keeping its latest report.
    bool divertNode(const char *id, uint32_t newDistanceM);

    // Returns the array index of node id, or -1 if it is not found.
    int indexOf(const char *id) const;

    uint8_t count() const {
        return count_;
    }

    bool full() const {
        return count_ >= MAX_NODES;
    }

    const RiverNode &at(uint8_t i) const {
        return nodes_[i];
    }

    // Entry point for the local engine, the simulator, or a future radio
    // link.
    bool updateReport(const char *id, const NodeReport &r, uint32_t nowMin);
    bool isFresh(const RiverNode &n, uint32_t nowMin) const;

    uint8_t positionCount() const;
    uint32_t positionDistance(uint8_t p) const;
    // Fills outIdx with indexes of nodes at distanceM; returns how many.
    uint8_t nodesAt(uint32_t distanceM, uint8_t *outIdx, uint8_t max) const;

    NetworkAssessment assess(const char *localId, uint32_t nowMin) const;

private:
    RiverNode nodes_[MAX_NODES];
    uint8_t count_;

    bool insertSorted(const RiverNode &n);
};
