#include "WifiMqttClient.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "Config.h"
#include "Database.h"
#include "BarrierController.h"
#include "RfidGates.h"

// =========================================================================
//  TRANG THAI NOI BO
// =========================================================================
namespace {
WiFiClient   espClient;
PubSubClient mqttClient(espClient);

uint32_t lastWifiCheckTime        = 0;
uint32_t lastMqttReconnectAttempt = 0;
uint32_t lastHeartbeatTime        = 0;

constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000;
constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000;
constexpr uint32_t HEARTBEAT_INTERVAL_MS      = 5000;

// =========================================================================
//  XU LY LENH TU WEB DASHBOARD (sub-routine cua mqttCallback)
// =========================================================================
void handleMqttCommand(const String& cmdJson) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, cmdJson);
    if (error) {
        Serial.println("[-] Giai ma JSON lenh MQTT that bai!");
        return;
    }

    String cmd = doc["cmd"].as<String>();

    // Lenh dieu khien barrier tu xa
    if (cmd == "open_gate_in") {
        openBarrierIn("lenh remote");
    } else if (cmd == "close_gate_in") {
        closeBarrierIn("lenh remote");
    } else if (cmd == "open_gate_out") {
        openBarrierOut("lenh remote");
    } else if (cmd == "close_gate_out") {
        closeBarrierOut("lenh remote");
    }
    // Lenh cap nhat / dong bo CSDL the
    else if (cmd == "add_user" || cmd == "update_user") {
        String uid = doc["uid"].as<String>();
        if (uid.length() > 0) {
            User u;
            bool exists = getUser(uid, u);
            if (!exists) {
                u.uid = uid;
                u.inParking = false;
                u.entryTime = "";
                u.status = "active";
            }
            if (doc.containsKey("name"))         u.name        = doc["name"].as<String>();
            if (doc.containsKey("studentId"))    u.studentId   = doc["studentId"].as<String>();
            if (doc.containsKey("plateNumber"))  u.plateNumber = doc["plateNumber"].as<String>();
            if (doc.containsKey("vehicleType"))  u.vehicleType = doc["vehicleType"].as<String>();
            if (doc.containsKey("balance"))      u.balance     = doc["balance"].as<long>();
            if (doc.containsKey("status"))       u.status      = doc["status"].as<String>();
            saveUser(u);
            Serial.printf("[+] Dong bo the %s thanh cong.\n", uid.c_str());
        }
    } else if (cmd == "update_balance") {
        String uid = doc["uid"].as<String>();
        long balance = doc["balance"].as<long>();
        User u;
        if (getUser(uid, u)) {
            Serial.printf("[SYNC] UID=%s BALANCE_LOCAL_BEFORE_SYNC=%ld BALANCE_SERVER=%ld\n",
                          uid.c_str(), u.balance, balance);
            u.balance = balance;
            saveUser(u);
            Serial.printf("[+] Dong bo so du the %s: %ld VND\n", uid.c_str(), balance);
        }
    } else if (cmd == "lock_card") {
        String uid = doc["uid"].as<String>();
        User u;
        if (getUser(uid, u)) {
            u.status = "locked";
            saveUser(u);
            Serial.printf("[+] Khoa the %s thanh cong.\n", uid.c_str());
        }
    } else if (cmd == "unlock_card") {
        String uid = doc["uid"].as<String>();
        User u;
        if (getUser(uid, u)) {
            u.status = "active";
            saveUser(u);
            Serial.printf("[+] Mo khoa the %s thanh cong.\n", uid.c_str());
        }
    } else if (cmd == "delete_user") {
        String uid = doc["uid"].as<String>();
        if (deleteUser(uid)) {
            Serial.printf("[+] Da xoa the %s khoi bo nho cuc bo.\n", uid.c_str());
        }
    }
}

// =========================================================================
//  CALLBACK NHAN MQTT MESSAGE
// =========================================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char payloadStr[length + 1];
    memcpy(payloadStr, payload, length);
    payloadStr[length] = '\0';

    Serial.printf("[MQTT] Nhan tin tu Topic: %s -> %s\n", topic, payloadStr);

    if (String(topic) == TOPIC_CMD) {
        handleMqttCommand(String(payloadStr));
    }
}

// =========================================================================
//  KET NOI WIFI / MQTT
// =========================================================================
void setupWiFi() {
    Serial.printf("\n[WiFi] Dang ket noi toi SSID: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 20) {
        delay(500);
        Serial.print(".");
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] KET NOI THANH CONG!");
        Serial.print("[WiFi] IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n[WiFi] KET NOI THAT BAI! System se hoat dong o che do Local Offline.");
    }
}

void reconnectMQTT() {
    Serial.printf("[MQTT] Dang thu ket noi toi Broker: %s...\n", MQTT_BROKER);
    String clientId = "ESP32ParkingSystem-" + String(random(0, 1000));

    bool connected = false;
    if (strlen(MQTT_USER) > 0) {
        connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS);
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }

    if (connected) {
        Serial.println("[MQTT] KET NOI THANH CONG!");
        mqttClient.subscribe(TOPIC_CMD);
        String responseTopic = String(TOPIC_PREFIX) + "/response/#";
        mqttClient.subscribe(responseTopic.c_str());
        sendDeviceStatus();
    } else {
        Serial.print("[MQTT] Ket noi that bai, rc=");
        Serial.println(mqttClient.state());
    }
}

void processOfflineQueue() {
    if (!mqttClient.connected()) return;

    String logPayload;
    while (getNextOfflineLog(logPayload)) {
        if (!mqttClient.publish(TOPIC_EVENT, logPayload.c_str())) {
            Serial.println("[-] Gui log offline len MQTT that bai.");
            break;
        }
        if (!popOfflineLog()) {
            Serial.println("[-] Xoa log offline da gui that bai.");
            break;
        }
        Serial.println("[+] Da dong bo 1 log offline len web.");
    }
}
}  // namespace

// =========================================================================
//  API PUBLIC
// =========================================================================
void initWifiMqtt() {
    setupWiFi();

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024); // Du cho JSON danh sach the
}

void maintainWifi(uint32_t currentMillis) {
    if (WiFi.status() == WL_CONNECTED) return;
    if (currentMillis - lastWifiCheckTime <= WIFI_RECONNECT_INTERVAL_MS) return;

    lastWifiCheckTime = currentMillis;
    Serial.println("[!] Mat ket noi WiFi! Dang thu ket noi lai...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void maintainMqtt(uint32_t currentMillis) {
    if (WiFi.status() != WL_CONNECTED) return;

    if (!mqttClient.connected()) {
        if (currentMillis - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL_MS) {
            lastMqttReconnectAttempt = currentMillis;
            reconnectMQTT();
        }
        return;
    }

    mqttClient.loop();
    processOfflineQueue();
}

void maintainHeartbeat(uint32_t currentMillis) {
    if (currentMillis - lastHeartbeatTime > HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatTime = currentMillis;
        sendDeviceStatus();
    }
}

bool isWifiConnected() { return WiFi.status() == WL_CONNECTED; }
bool isMqttConnected() { return mqttClient.connected(); }

// =========================================================================
//  PUBLISH HELPERS
// =========================================================================
void sendDeviceStatus() {
    if (!mqttClient.connected()) return;

    StaticJsonDocument<384> doc;
    const bool allRfidReady = isRfidInReady() && isRfidOutReady();

    doc["esp32"]       = "online";
    doc["wifi"]        = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";
    doc["mqtt"]        = mqttClient.connected() ? "connected" : "disconnected";
    doc["rc522"]       = allRfidReady ? "connected" : "disconnected";
    doc["pn532"]       = allRfidReady ? "connected" : "disconnected";
    doc["rfid_in"]     = isRfidInReady() ? "connected" : "disconnected";
    doc["rfid_out"]    = isRfidOutReady() ? "connected" : "disconnected";
    doc["barrier_in"]  = getBarrierInState();
    doc["barrier_out"] = getBarrierOutState();

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(TOPIC_STATUS, payload.c_str());
}

void publishParkingEvent(const String& uid, const String& event, const String& timeStr,
                         long fee, long balance) {
    StaticJsonDocument<256> doc;
    if (uid.length() > 0) doc["uid"] = uid;
    doc["event"] = event;
    doc["time"]  = timeStr;
    if (fee >= 0)     doc["fee"]     = fee;
    if (balance >= 0) doc["balance"] = balance;

    String payload;
    serializeJson(doc, payload);

    if (mqttClient.connected()) {
        mqttClient.publish(TOPIC_EVENT, payload.c_str());
        return;
    }

    // Luu queue ngoai tuyen de gui khi co mang tro lai
    queueOfflineLog(uid, event,
                    fee >= 0 ? fee : 0,
                    balance >= 0 ? balance : 0,
                    timeStr,
                    fee >= 0 || balance >= 0);
}

void publishScan(const String& uid) {
    if (mqttClient.connected()) {
        mqttClient.publish(TOPIC_SCAN, uid.c_str());
    }
}
