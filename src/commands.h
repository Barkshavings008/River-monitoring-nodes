#pragma once
// Serial commands for editing the node network at runtime:
//   list | add <id> <metres> [place] | del <id> | move <id> <metres> | cal | help
#include "network.h"

void commandsPoll(RiverNetwork& net, uint32_t nowMin);
