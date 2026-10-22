#include "node_link.h"
#include <string.h>

// The ESP-NOW radio side of node_link.h (the packing is in node_link.cpp)

/////////////////////////
//////// RADIO //////////
/////////////////////////

#if NODE_LINK && defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define LINK_QUEUE 8

QueueHandle_t link_queue = NULL;
const uint8_t broadcast_address[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Called by the WiFi task when a packet arrives: just queue it for loop()
void on_packet(const uint8_t *mac, const uint8_t *data, int length) {
    if (link_queue == NULL || length != (int)sizeof(struct Node_packet)) {
        return;
    }
    xQueueSend(link_queue, data, 0);
}

void node_link_begin(void) {
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    }
    // Without WiFi notifications, stay on a fixed channel so the boards meet
    if (WIFI_SSID_1[0] == '\0') {
        esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    }
    if (esp_now_init() != ESP_OK) {
        Serial.println("Node link: ESP-NOW didn't start");
        return;
    }
    link_queue = xQueueCreate(LINK_QUEUE, sizeof(struct Node_packet));
    esp_now_register_recv_cb(on_packet);

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, broadcast_address, 6);
    peer.channel = 0;          // whatever channel WiFi is on
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

// Broadcasts this node's report to every board in range
void node_link_send(struct Node_report report) {
    if (link_queue == NULL) {
        return;
    }
    struct Node_packet packet;
    pack_report(&packet, report, NODE_DISTANCE_M, NODE_PLACE);
    esp_now_send(broadcast_address, (const uint8_t *)&packet, sizeof(struct Node_packet));
}

// Gets the next received packet, if there is one
bool node_link_receive(uint8_t *data, int *length) {
    if (link_queue == NULL) {
        return false;
    }
    if (xQueueReceive(link_queue, data, 0) != pdTRUE) {
        return false;
    }
    *length = sizeof(struct Node_packet);
    return true;
}

#else

// Link turned off (or a PC build): nothing to do
void node_link_begin(void) {
}

void node_link_send(struct Node_report report) {
}

bool node_link_receive(uint8_t *data, int *length) {
    return false;
}

#endif
