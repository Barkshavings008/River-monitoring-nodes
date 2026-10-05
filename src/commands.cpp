#include "commands.h"
#include "display.h"
#include "sensors.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#define COMMAND_LINE_SIZE 80
#define MESSAGE_SIZE 96

char command_line[COMMAND_LINE_SIZE];  // the command being typed in
int command_length = 0;

////////////////////////
// Function prototypes//
////////////////////////
void run_command(char *command, struct River_network *network, unsigned long now_min);
void add_command(struct River_network *network, unsigned long now_min);
void del_command(struct River_network *network, unsigned long now_min);
void move_command(struct River_network *network, unsigned long now_min);
bool read_metres(const char *text, unsigned long *metres);
////////////////////////

// Reads whatever has been typed into the serial monitor. Once a whole line
// (ending in Enter) has come in, it gets run.
void check_serial_commands(struct River_network *network, unsigned long now_min) {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            command_line[command_length] = '\0';
            run_command(command_line, network, now_min);
            command_length = 0;
        } else if (c != '\r' && command_length < COMMAND_LINE_SIZE - 1) {
            command_line[command_length] = c;
            command_length++;
        }
    }
}

/////////////////////////
// Function definitions//
/////////////////////////

// strtok() splits the line up into words. The first call gets the command
// word, then each strtok(NULL, ...) call in the other functions gets the next word.
void run_command(char *command, struct River_network *network, unsigned long now_min) {
    char *word = strtok(command, " ");
    if (word == NULL) {
        return;
    }

    if (strcmp(word, "list") == 0) {
        print_network_list(network, now_min);
    } else if (strcmp(word, "add") == 0) {
        add_command(network, now_min);
    } else if (strcmp(word, "del") == 0) {
        del_command(network, now_min);
    } else if (strcmp(word, "move") == 0) {
        move_command(network, now_min);
    } else if (strcmp(word, "cal") == 0) {
        if (SIMULATE) {
            print_message("cal: not available in SIMULATE mode");
        } else {
            print_voltages(last_sensor_voltages());
        }
    } else {
        print_help();
    }
}

// add <id> <metres> [place]
void add_command(struct River_network *network, unsigned long now_min) {
    char message[MESSAGE_SIZE];
    char *id = strtok(NULL, " ");
    char *distance = strtok(NULL, " ");
    char *place = strtok(NULL, "");   // "" = the rest of the line
    unsigned long metres;
    if (id == NULL || !read_metres(distance, &metres)) {
        print_message("Usage: add <id> <metres> [place]");
        return;
    }

    // Skip spaces and quotes at the start, and cut the place off at a closing quote
    if (place != NULL) {
        while (place[0] == ' ' || place[0] == '"') {
            place++;
        }
        char *quote = strchr(place, '"');
        if (quote != NULL) {
            quote[0] = '\0';
        }
    }
    const char *place_name = "added";
    if (place != NULL && place[0] != '\0') {
        place_name = place;
    }

    if (network_add_node(network, id, metres, place_name, false)) {
        snprintf(message, MESSAGE_SIZE, "Added %s at %u m", id, (unsigned)metres);
    } else {
        snprintf(message, MESSAGE_SIZE, "Could not add %s (id over %d characters, already used, or network full)",
                 id, NODE_ID_LEN - 1);
    }
    print_message(message);
    print_network_list(network, now_min);
}

// del <id>
void del_command(struct River_network *network, unsigned long now_min) {
    char message[MESSAGE_SIZE];
    char *id = strtok(NULL, " ");
    int index = network_find_node(network, id);
    if (index < 0) {
        print_message("Unknown node");
        return;
    }
    if (network->nodes[index].is_local) {
        print_message("Cannot delete this board's own node");
        return;
    }
    network_remove_node(network, id);
    snprintf(message, MESSAGE_SIZE, "Deleted %s", id);
    print_message(message);
    print_network_list(network, now_min);
}

// move <id> <metres>
void move_command(struct River_network *network, unsigned long now_min) {
    char message[MESSAGE_SIZE];
    char *id = strtok(NULL, " ");
    char *distance = strtok(NULL, " ");
    unsigned long metres;
    if (id == NULL || !read_metres(distance, &metres)) {
        print_message("Usage: move <id> <metres>");
        return;
    }
    if (network_move_node(network, id, metres)) {
        snprintf(message, MESSAGE_SIZE, "Moved %s to %u m", id, (unsigned)metres);
    } else {
        snprintf(message, MESSAGE_SIZE, "Unknown node %s", id);
    }
    print_message(message);
    print_network_list(network, now_min);
}

// Turns text like "1500" into a number. Returns false if it's empty or not a number.
// Only plain digits are accepted (no minus sign), and at most 9 of them so the
// number can't overflow.
bool read_metres(const char *text, unsigned long *metres) {
    if (text == NULL || text[0] == '\0' || strlen(text) > 9) {
        return false;
    }
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    *metres = strtoul(text, NULL, 10);
    return true;
}
