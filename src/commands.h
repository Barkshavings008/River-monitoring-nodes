#pragma once
// Serial commands for changing the node network while it's running:
//   list | add <id> <metres> [place] | del <id> | move <id> <metres> | cal | help
#include "network.h"

////////////////////////
// Function prototypes//
////////////////////////
void check_serial_commands(struct River_network *network, unsigned long now_min);
////////////////////////
