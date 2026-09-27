#include "network.h"
#include <string.h>

namespace {

void copyStr(char* dst, const char* src, size_t size) {
  strncpy(dst, src ? src : "", size - 1);
  dst[size - 1] = '\0';
}

}  // namespace

bool RiverNetwork::insertSorted(const RiverNode& n) {
  if (full()) return false;
  uint8_t i = 0;
  while (i < count_ && nodes_[i].distanceM <= n.distanceM) i++;
  for (uint8_t j = count_; j > i; j--) nodes_[j] = nodes_[j - 1];
  nodes_[i] = n;
  count_++;
  return true;
}

bool RiverNetwork::addNode(const char* id, uint32_t distanceM, const char* place, bool isLocal) {
  if (!id || !id[0] || indexOf(id) >= 0 || full()) return false;
  RiverNode n;
  memset(&n, 0, sizeof(n));
  copyStr(n.id, id, sizeof(n.id));
  copyStr(n.place, place, sizeof(n.place));
  n.distanceM = distanceM;
  n.isLocal = isLocal;
  return insertSorted(n);
}

bool RiverNetwork::removeNode(const char* id) {
  int i = indexOf(id);
  if (i < 0) return false;
  for (uint8_t j = i; j + 1 < count_; j++) nodes_[j] = nodes_[j + 1];
  count_--;
  return true;
}

bool RiverNetwork::divertNode(const char* id, uint32_t newDistanceM) {
  int i = indexOf(id);
  if (i < 0) return false;
  RiverNode n = nodes_[i];
  removeNode(id);
  n.distanceM = newDistanceM;
  return insertSorted(n);
}

int RiverNetwork::indexOf(const char* id) const {
  if (!id) return -1;
  for (uint8_t i = 0; i < count_; i++)
    if (strncmp(nodes_[i].id, id, NODE_ID_LEN) == 0) return i;
  return -1;
}

bool RiverNetwork::updateReport(const char* id, const NodeReport& r, uint32_t nowMin) {
  int i = indexOf(id);
  if (i < 0) return false;
  nodes_[i].report = r;
  nodes_[i].hasReport = true;
  nodes_[i].lastUpdateMin = nowMin;
  return true;
}

bool RiverNetwork::isFresh(const RiverNode& n, uint32_t nowMin) const {
  return n.hasReport && nowMin - n.lastUpdateMin <= NODE_STALE_MIN;
}

uint8_t RiverNetwork::positionCount() const {
  uint8_t p = 0;
  for (uint8_t i = 0; i < count_; i++)
    if (i == 0 || nodes_[i].distanceM != nodes_[i - 1].distanceM) p++;
  return p;
}

uint32_t RiverNetwork::positionDistance(uint8_t p) const {
  uint8_t seen = 0;
  for (uint8_t i = 0; i < count_; i++) {
    if (i > 0 && nodes_[i].distanceM == nodes_[i - 1].distanceM) continue;
    if (seen++ == p) return nodes_[i].distanceM;
  }
  return 0;
}

uint8_t RiverNetwork::nodesAt(uint32_t distanceM, uint8_t* outIdx, uint8_t max) const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < count_ && n < max; i++)
    if (nodes_[i].distanceM == distanceM) outIdx[n++] = i;
  return n;
}

NetworkAssessment RiverNetwork::assess(const char* localId, uint32_t nowMin) const {
  NetworkAssessment a;
  memset(&a, 0, sizeof(a));
  a.finding = NF_NONE;
  copyStr(a.localId, localId, sizeof(a.localId));

  int li = indexOf(localId);
  if (li < 0 || !nodes_[li].hasReport) return a;
  const RiverNode& local = nodes_[li];
  const NodeReport& lr = local.report;
  a.localDist = local.distanceM;

  // Count fresh peers, siblings and rain patterns.
  for (uint8_t i = 0; i < count_; i++) {
    if ((int)i == li || !isFresh(nodes_[i], nowMin)) continue;
    const NodeReport& r = nodes_[i].report;
    a.peers++;
    if (r.rainPattern || reportHasLabel(r, LBL_RAIN)) a.rainPeers++;
    if (nodes_[i].distanceM == local.distanceM) {
      a.siblings++;
      if (r.state == ST_ALERT) a.siblingsAlerting++;
    }
  }

  // Nearest upstream position that has fresh data. Array is sorted, so walk back.
  bool upstreamSame = false;
  for (int i = li - 1; i >= 0; i--) {
    if (nodes_[i].distanceM == local.distanceM || !isFresh(nodes_[i], nowMin)) continue;
    if (!a.hasUpstream) {
      a.hasUpstream = true;
      a.upstreamDist = nodes_[i].distanceM;
      copyStr(a.upstreamId, nodes_[i].id, sizeof(a.upstreamId));
    } else if (nodes_[i].distanceM != a.upstreamDist) {
      break;
    }
    const NodeReport& r = nodes_[i].report;
    if (r.state == ST_ALERT && r.label == lr.label) upstreamSame = true;
  }

  bool localRain = lr.rainPattern || reportHasLabel(lr, LBL_RAIN);
  if (lr.state == ST_ALERT) {
    if (upstreamSame) a.finding = NF_FROM_UPSTREAM;
    else if (a.siblings > 0 && a.siblingsAlerting == a.siblings) a.finding = NF_SOURCE_CROSS_SECTION;
    else if (a.siblings > 0) a.finding = NF_SOURCE_LOCAL_SIDE;
    else if (a.hasUpstream) a.finding = NF_SOURCE_BETWEEN;
    else a.finding = NF_NO_PEERS;
  } else if (localRain) {
    // Rain hits every node at once; a leak shows up at one node first.
    if (a.peers == 0) a.finding = NF_NO_PEERS;
    else if ((a.rainPeers + 1) * 2 > a.peers + 1) a.finding = NF_RAIN_CONFIRMED;
    else a.finding = NF_RAIN_UNCONFIRMED;
  }
  return a;
}
