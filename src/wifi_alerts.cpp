#include "wifi_alerts.h"
#include "config.h"
#include <string.h>

#if WIFI_ALERTS && defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#define NOTIFY_TASK_STACK 12288
#define WIFI_CONNECT_TIMEOUT_MS 8000
#define RETRY_WAIT_MS 3000

WiFiMulti wifi_multi;
QueueHandle_t notify_queue = NULL;

// Set by the background task, read by loop()
volatile bool wifi_up = false;
volatile int sent_count = 0;
volatile int failed_count = 0;
volatile int last_http_code = 0;
char wifi_network[33] = "";
char wifi_ip[16] = "";

////////////////////////
// Function prototypes//
////////////////////////
void notify_task(void *unused);
int post_to_ntfy(const char *json);
////////////////////////

void wifi_alerts_begin(void) {
    if (!wifi_alerts_enabled()) {
        return;
    }
    WiFi.mode(WIFI_STA);
    wifi_multi.addAP(WIFI_SSID_1, WIFI_PASSWORD_1);
    if (WIFI_SSID_2[0] != '\0') {
        wifi_multi.addAP(WIFI_SSID_2, WIFI_PASSWORD_2);
    }
    notify_queue = xQueueCreate(NOTIFY_QUEUE, NOTIFY_SIZE);
    // Core 0 is where WiFi and Bluetooth run; loop() runs on core 1
    xTaskCreatePinnedToCore(notify_task, "notify", NOTIFY_TASK_STACK, NULL, 1, NULL, 0);
}

bool wifi_alerts_enabled(void) {
    return WIFI_SSID_1[0] != '\0' && NTFY_TOPIC[0] != '\0';
}

bool wifi_alerts_connected(void) {
    return wifi_up;
}

// Puts a notification in the queue. Returns false if WiFi is off or the
// queue is full (it never waits, so loop() keeps running).
bool wifi_alerts_send(const char *json) {
    if (notify_queue == NULL || json == NULL || json[0] == '\0') {
        return false;
    }
    static char item[NOTIFY_SIZE];   // the queue copies exactly NOTIFY_SIZE bytes
    strncpy(item, json, NOTIFY_SIZE - 1);
    item[NOTIFY_SIZE - 1] = '\0';
    return xQueueSend(notify_queue, item, 0) == pdTRUE;
}

struct Wifi_status wifi_alerts_status(void) {
    struct Wifi_status status;
    memset(&status, 0, sizeof(struct Wifi_status));
    status.enabled = wifi_alerts_enabled();
    status.connected = wifi_up;
    strncpy(status.network, wifi_network, sizeof(status.network) - 1);
    strncpy(status.ip, wifi_ip, sizeof(status.ip) - 1);
    status.sent = sent_count;
    status.failed = failed_count;
    status.last_code = last_http_code;
    if (notify_queue != NULL) {
        status.waiting = uxQueueMessagesWaiting(notify_queue);
    }
    status.free_memory = ESP.getFreeHeap();
    return status;
}

/////////////////////////
// Function definitions//
/////////////////////////

// Runs forever in the background: keeps WiFi connected and sends whatever is
// in the queue. A notification only leaves the queue once ntfy accepts it,
// or after NOTIFY_TRIES failed attempts.
void notify_task(void *unused) {
    static char message[NOTIFY_SIZE];
    int tries = 0;
    for (;;) {
        if (wifi_multi.run(WIFI_CONNECT_TIMEOUT_MS) != WL_CONNECTED) {
            wifi_up = false;
            vTaskDelay(pdMS_TO_TICKS(RETRY_WAIT_MS));
            continue;
        }
        if (!wifi_up) {
            strncpy(wifi_network, WiFi.SSID().c_str(), sizeof(wifi_network) - 1);
            strncpy(wifi_ip, WiFi.localIP().toString().c_str(), sizeof(wifi_ip) - 1);
            wifi_up = true;
        }

        if (xQueuePeek(notify_queue, message, pdMS_TO_TICKS(1000)) != pdTRUE) {
            continue;   // nothing to send
        }
        int code = post_to_ntfy(message);
        last_http_code = code;
        if (code == 200) {
            xQueueReceive(notify_queue, message, 0);
            sent_count++;
            tries = 0;
        } else {
            tries++;
            if (tries >= NOTIFY_TRIES) {
                xQueueReceive(notify_queue, message, 0);
                failed_count++;
                tries = 0;
            } else {
                vTaskDelay(pdMS_TO_TICKS(RETRY_WAIT_MS));
            }
        }
    }
}

// Posts one notification. Returns the HTTP code (200 = OK, below 0 = no
// connection). The certificate isn't checked (setInsecure) to keep it
// simple: the messages are alerts, not secrets.
int post_to_ntfy(const char *json) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, NTFY_SERVER)) {
        return -1;
    }
    http.setTimeout(10000);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST((uint8_t *)json, strlen(json));
    http.end();
    return code;
}

#else

// WiFi turned off (or a PC build): nothing to do
void wifi_alerts_begin(void) {
}

bool wifi_alerts_enabled(void) {
    return false;
}

bool wifi_alerts_connected(void) {
    return false;
}

bool wifi_alerts_send(const char *json) {
    return false;
}

struct Wifi_status wifi_alerts_status(void) {
    struct Wifi_status status;
    memset(&status, 0, sizeof(struct Wifi_status));
    return status;
}

#endif
