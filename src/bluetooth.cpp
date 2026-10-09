#include "bluetooth.h"
#include "config.h"
#include <string.h>

#if BLUETOOTH && defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <NimBLEDevice.h>

// Nordic UART service: phone apps and the phone page look for these
#define UART_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"   // phone writes here
#define UART_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"   // board notifies here

#define REQUEST_SIZE 32
#define MIN_PIECE 20          // smallest Bluetooth packet (MTU 23 - 3)
#define MAX_PIECE 240
#define SEND_TRIES 5

NimBLEServer *ble_server = NULL;
NimBLECharacteristic *tx_characteristic = NULL;

// Set from the Bluetooth task, read in loop()
volatile bool phone_connected = false;
volatile bool new_connection = false;
volatile int piece_size = MIN_PIECE;
volatile bool request_waiting = false;
char request_text[REQUEST_SIZE];

// Called by NimBLE when a phone connects or disconnects
class Server_events : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override {
        phone_connected = true;
        new_connection = true;
        piece_size = MIN_PIECE;
    }
    void onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason) override {
        phone_connected = false;
        // NimBLE starts advertising again by itself so the phone can reconnect
    }
    // Phones agree on a bigger packet size after connecting (iPhones: 185 bytes)
    void onMTUChange(uint16_t mtu, NimBLEConnInfo &info) override {
        int piece = mtu - 3;
        if (piece < MIN_PIECE) {
            piece = MIN_PIECE;
        }
        if (piece > MAX_PIECE) {
            piece = MAX_PIECE;
        }
        piece_size = piece;
    }
};

// Called by NimBLE when the phone writes to RX
class Rx_events : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override {
        if (request_waiting) {
            return; // loop() hasn't dealt with the last one yet
        }
        NimBLEAttValue value = characteristic->getValue();
        int length = value.length();
        if (length > REQUEST_SIZE - 1) {
            length = REQUEST_SIZE - 1;
        }
        memcpy(request_text, value.data(), length);
        request_text[length] = '\0';
        request_waiting = true;
    }
};

Server_events server_events;
Rx_events rx_events;

void bluetooth_begin(const char *node_id) {
    char name[32];
    snprintf(name, 32, "%s%s", BLE_NAME_PREFIX, node_id);

    NimBLEDevice::init(name);
    NimBLEDevice::setMTU(MAX_PIECE + 3);
    ble_server = NimBLEDevice::createServer();
    ble_server->setCallbacks(&server_events);
    ble_server->advertiseOnDisconnect(true);

    NimBLEService *service = ble_server->createService(UART_SERVICE_UUID);
    tx_characteristic = service->createCharacteristic(UART_TX_UUID, NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic *rx = service->createCharacteristic(UART_RX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rx->setCallbacks(&rx_events);
    // (the service starts when advertising starts)

    // The long service id fills most of the advert, so the name goes in the
    // scan response (the second packet a phone asks for while scanning)
    NimBLEAdvertisementData advert;
    advert.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advert.addServiceUUID(UART_SERVICE_UUID);
    NimBLEAdvertisementData scan_response;
    scan_response.setName(name);

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    advertising->setAdvertisementData(advert);
    advertising->setScanResponseData(scan_response);
    advertising->enableScanResponse(true);
    advertising->start();
}

bool bluetooth_connected(void) {
    return phone_connected;
}

// true once each time a phone connects (so loop() can send it everything straight away)
bool bluetooth_just_connected(void) {
    if (new_connection) {
        new_connection = false;
        return true;
    }
    return false;
}

// Sends one message in pieces that fit in a Bluetooth packet. If the phone's
// buffer is full a piece is tried again a few times before giving up.
void bluetooth_send_line(const char *line, int length) {
    if (!phone_connected || tx_characteristic == NULL || length <= 0) {
        return;
    }
    int sent = 0;
    while (sent < length && phone_connected) {
        int piece = length - sent;
        if (piece > piece_size) {
            piece = piece_size;
        }
        bool ok = false;
        for (int tries = 0; tries < SEND_TRIES && !ok; tries++) {
            ok = tx_characteristic->notify((const uint8_t *)line + sent, piece);
            if (!ok) {
                delay(10);
            }
        }
        if (!ok) {
            return;
        }
        sent += piece;
        delay(2);
    }
}

// If the phone sent a request, copies it into request and returns true
bool bluetooth_take_request(char *request, int size) {
    if (!request_waiting) {
        return false;
    }
    strncpy(request, request_text, size - 1);
    request[size - 1] = '\0';
    // Drop the Enter the phone may have added
    int length = strlen(request);
    while (length > 0 && (request[length - 1] == '\n' || request[length - 1] == '\r')) {
        request[length - 1] = '\0';
        length--;
    }
    request_waiting = false;
    return true;
}

#else

// Bluetooth turned off (or a PC build): nothing to do
void bluetooth_begin(const char *node_id) {
}

bool bluetooth_connected(void) {
    return false;
}

bool bluetooth_just_connected(void) {
    return false;
}

void bluetooth_send_line(const char *line, int length) {
}

bool bluetooth_take_request(char *request, int size) {
    return false;
}

#endif
