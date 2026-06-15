#ifndef WIFI_MQTT_CLIENT_H
#define WIFI_MQTT_CLIENT_H

#include <Arduino.h>
//  WifiMqttClient
//  ------------------------------------------------------------
//  Bao gom tat ca cong viec lien quan MQTT/WiFi cua he thong:
//      - Ket noi WiFi (blocking ~10s khi setup; non-blocking trong loop)
//      - Ket noi MQTT broker + reconnect + subscribe topics
//      - Xu ly lenh tu Web Dashboard (handle MQTT callback)
//      - Heartbeat dinh ky (sendDeviceStatus / 5s)
//      - Dong bo queue log ngoai tuyen khi co mang tro lai
//      - Helper publish event check-in/out / scan UID
//
//  main.cpp chi can goi:
//      initWifiMqtt() trong setup()
//      maintainWifi(currentMillis); maintainMqtt(currentMillis);
//      maintainHeartbeat(currentMillis); trong loop()

// Khoi tao WiFi + MQTT broker (chua connect MQTT). Goi mot lan tu setup().
void initWifiMqtt();

// Reconnect WiFi non-blocking moi 10s neu mat ket noi
void maintainWifi(uint32_t currentMillis);

// Duy tri MQTT: reconnect 5s, mqttClient.loop(), xu ly offline queue
void maintainMqtt(uint32_t currentMillis);

// Gui heartbeat status moi 5s
void maintainHeartbeat(uint32_t currentMillis);

// Trang thai ket noi
bool isWifiConnected();
bool isMqttConnected();

// Publish helpers
void sendDeviceStatus();                                          // Heartbeat
void publishParkingEvent(const String& uid, const String& event,  // Event check-in/out/manual
                         const String& timeStr,
                         long fee = -1, long balance = -1);
void publishScan(const String& uid);                              // The chua dang ky

#endif // WIFI_MQTT_CLIENT_H
