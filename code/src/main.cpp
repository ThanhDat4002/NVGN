#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <RTClib.h>
#include <ESP32Servo.h>

#include "Config.h"
#include "Database.h"
#include "Buzzer.h"
#include "ParkingDisplay.h"

// ==========================================
// ĐỐI TƯỢNG VÀ BIẾN TOÀN CỤC
// ==========================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

RTC_DS3231 rtc;

Servo servoIn;
Servo servoOut;

// Hai đầu đọc RC522 sử dụng hardware SPI
#include <MFRC522.h>
MFRC522 nfcIn(RC522_IN_SS, RC522_RST_PIN);
MFRC522 nfcOut(RC522_OUT_SS, RC522_RST_PIN);

// Trạng thái phần cứng phục vụ cho Heartbeat
bool isRfidInOk = false;
bool isRfidOutOk = false;
bool isRtcOk = false;
DateTime rtcFallbackBase;
uint32_t rtcFallbackStartMillis = 0;

// Trạng thái Barrier (cổng vào/ra)
String barrierInState = "closed";
String barrierOutState = "closed";
uint32_t barrierInCloseTime = 0;
uint32_t barrierOutCloseTime = 0;

// Bộ đệm chống quét đúp thẻ trong thời gian ngắn
String lastScannedUid = "";
uint32_t lastScanTime = 0;
#define DEBOUNCE_TIME_MS  5000

// Các bộ hẹn giờ phi chặn (Non-blocking timers)
uint32_t lastHeartbeatTime = 0;
uint32_t lastWifiCheckTime = 0;
uint32_t lastMqttReconnectAttempt = 0;
uint32_t displayResetTime = 0;
bool isDisplayActiveMessage = false;
uint32_t manualInPressStart = 0;
uint32_t manualOutPressStart = 0;
bool manualInLongPressHandled = false;
bool manualOutLongPressHandled = false;

// ==========================================
// KHAI BÁO CÁC HÀM CON
// ==========================================
void setupWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleMqttCommand(const String& cmdJson);
void sendDeviceStatus();
void processOfflineQueue();
void checkRfidGates();
void processCheckIn(const String& uid);
void processCheckOut(const String& uid);
long calculateParkingFee(const String& entryTimeStr, const DateTime& exitTime);
bool parseDateTime(const String& str, int& y, int& m, int& d, int& h, int& min);
String getFormattedTime(const DateTime& dt);
DateTime getCurrentDateTime();
bool initRc522Reader(MFRC522& reader, const char* gateLabel);
void resetRc522Bus();
void handleManualButtons(uint32_t currentMillis);
void openBarrierIn(const char* source);
void closeBarrierIn(const char* source);
void openBarrierOut(const char* source);
void closeBarrierOut(const char* source);
void publishParkingEvent(const String& uid, const String& event, const String& timeStr, long fee = -1, long balance = -1);

// ==========================================
// HÀM SETUP KHỞI CHẠY HỆ THỐNG
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== HE THONG QUAN LY BAI XE THONG MINH ===");

    // Khởi tạo các chân chọn Chip Select SPI mức HIGH trước để chống xung đột bus
    pinMode(RC522_IN_SS, OUTPUT);
    pinMode(RC522_OUT_SS, OUTPUT);
    pinMode(RC522_RST_PIN, OUTPUT);
    pinMode(TFT_CS, OUTPUT);
    pinMode(MANUAL_OPEN_IN_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MANUAL_OPEN_OUT_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(RC522_IN_SS, HIGH);
    digitalWrite(RC522_OUT_SS, HIGH);
    digitalWrite(RC522_RST_PIN, HIGH);
    digitalWrite(TFT_CS, HIGH);

    // Khởi động SPI phần cứng với chân cấu hình tùy chỉnh ngay từ đầu
    SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

    // 1. Khởi tạo còi Buzzzer
    initBuzzer();
    playTone(1500, 100); // Kêu bíp nhẹ chào mừng

    // 2. Khởi tạo màn hình TFT ST7735
    initDisplay();
    showBootingScreen("1. Hardware Init...", 10);
    delay(500);

    // 3. Khởi tạo cơ sở dữ liệu LittleFS
    showBootingScreen("2. LittleFS Init...", 30);
    if (!initDatabase()) {
        showWarningScreen("LittleFS Error!");
        beepError();
        while (1) delay(100);
    }
    delay(500);

    // 4. Khởi tạo RTC DS3231 (I2C)
    showBootingScreen("3. RTC DS3231 Init...", 50);
    Wire.begin(I2C_SDA, I2C_SCL);
    rtcFallbackBase = DateTime(F(__DATE__), F(__TIME__));
    rtcFallbackStartMillis = millis();
    isRtcOk = rtc.begin();
    if (!isRtcOk) {
        Serial.println("[-] Khong tim thay RTC DS3231!");
        Serial.println("[!] Su dung moc thoi gian fallback tu thoi diem nap firmware.");
    } else {
        if (rtc.lostPower()) {
            Serial.println("[!] RTC mat nguon! Dang thiet lap lai gio compile.");
            rtc.adjust(rtcFallbackBase);
        }
        Serial.println("[+] RTC DS3231 khoi dong ok.");
    }
    delay(500);

    // 5. Khởi tạo Servo Barrier
    showBootingScreen("4. Servo Init...", 70);
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    servoIn.setPeriodHertz(50);
    servoOut.setPeriodHertz(50);
    
    servoIn.attach(SERVO_IN_PIN, 500, 2400);
    servoOut.attach(SERVO_OUT_PIN, 500, 2400);
    
    // Đặt barrier ở trạng thái đóng mặc định
    servoIn.write(BARRIER_CLOSED_ANGLE);
    servoOut.write(BARRIER_CLOSED_ANGLE);
    delay(500);

    // 6. Khởi tạo các đầu đọc RFID RC522 (SPI)
    showBootingScreen("5. RFID RC522 Init...", 85);

    isRfidInOk = initRc522Reader(nfcIn, "Cong Vao");
    isRfidOutOk = initRc522Reader(nfcOut, "Cong Ra");
    delay(500);

    // 7. Kết nối WiFi
    showBootingScreen("6. Connecting WiFi...", 95);
    setupWiFi();

    // Cài đặt MQTT Broker
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024); // Đảm bảo đệm đủ lớn cho JSON danh sách thẻ

    // Báo kết thúc booting thành công
    showBootingScreen("Initialization Complete!", 100);
    playTone(2000, 150);
    delay(1000);
}

// ==========================================
// VÒNG LẶP CHÍNH (LOOP)
// ==========================================
void loop() {
    uint32_t currentMillis = millis();

    // 1. Duy trì kết nối WiFi (Kiểm tra định kỳ phi chặn mỗi 10 giây)
    if (WiFi.status() != WL_CONNECTED) {
        if (currentMillis - lastWifiCheckTime > 10000) {
            lastWifiCheckTime = currentMillis;
            Serial.println("[!] Mat ket noi WiFi! Dang thu ket noi lai...");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    }

    // 2. Duy trì kết nối MQTT Broker
    if (WiFi.status() == WL_CONNECTED) {
        if (!mqttClient.connected()) {
            if (currentMillis - lastMqttReconnectAttempt > 5000) {
                lastMqttReconnectAttempt = currentMillis;
                reconnectMQTT();
            }
        } else {
            mqttClient.loop();
            // Đồng bộ dữ liệu ngoại tuyến khi có mạng trở lại
            processOfflineQueue();
        }
    }

    // 3. Gửi tin nhắn trạng thái thiết bị (Heartbeat) định kỳ mỗi 5 giây
    if (currentMillis - lastHeartbeatTime > 5000) {
        lastHeartbeatTime = currentMillis;
        sendDeviceStatus();
    }

    // 4. Tự động đóng cổng (Servo Barrier) sau khi mở quá thời gian delay
    if (barrierInState == "open" && currentMillis > barrierInCloseTime) {
        closeBarrierIn("tu dong");
    }
    if (barrierOutState == "open" && currentMillis > barrierOutCloseTime) {
        closeBarrierOut("tu dong");
    }

    // 5. Reset màn hình hiển thị về màn hình chờ mặc định sau 4 giây hiển thị thông báo checkin/checkout/lỗi
    if (isDisplayActiveMessage && currentMillis > displayResetTime) {
        isDisplayActiveMessage = false;
    }

    // 6. Cập nhật giao diện màn hình Chờ (Idle Screen)
    if (!isDisplayActiveMessage) {
        DateTime nowDt = getCurrentDateTime();
        String dtStr = getFormattedTime(nowDt);
        
        // Đếm số lượng xe trong bãi
        int parkedCount = 0;
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file) {
            String filename = file.name();
            if (filename.startsWith("/u_")) {
                file.close();
                User testUser;
                String uidFromFile = filename.substring(3, filename.length() - 5);
                if (getUser(uidFromFile, testUser) && testUser.inParking) {
                    parkedCount++;
                }
            } else {
                file.close();
            }
            file = root.openNextFile();
        }
        
        showIdleScreen(dtStr, parkedCount, WiFi.status() == WL_CONNECTED, mqttClient.connected());
    }

    // 7. Quét các thẻ NFC tại 2 đầu đọc cổng vào/ra
    handleManualButtons(currentMillis);
    checkRfidGates();

    // Delay ngắn để tránh quá nhiệt và nhường quyền xử lý luồng WiFi
    delay(10);
}

// ==========================================
// CẤU HÌNH KẾT NỐI WIFI
// ==========================================
void setupWiFi() {
    Serial.printf("\n[WiFi] Dang ket noi toi SSID: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempt = 0;
    // Đợi kết nối WiFi tối đa 10 giây trong lúc setup, nếu không sẽ chạy tiếp chế độ Offline-First
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

// ==========================================
// DUY TRÌ KẾT NỐI MQTT BROKER
// ==========================================
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
        // Đăng ký nhận lệnh từ Web Dashboard
        mqttClient.subscribe(TOPIC_CMD);
        // Đăng ký nhận phản hồi trực tiếp của thẻ
        String responseTopic = String(TOPIC_PREFIX) + "/response/#";
        mqttClient.subscribe(responseTopic.c_str());
        
        sendDeviceStatus();
    } else {
        Serial.print("[MQTT] Ket noi that bai, rc=");
        Serial.println(mqttClient.state());
    }
}

// ==========================================
// BỘ LẮNG NGHE SỰ KIỆN MQTT CALLBACK (SUB)
// ==========================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char payloadStr[length + 1];
    memcpy(payloadStr, payload, length);
    payloadStr[length] = '\0';
    
    Serial.printf("[MQTT] Nhan tin tu Topic: %s -> %s\n", topic, payloadStr);

    String topicStr = String(topic);
    
    // 1. Lệnh từ Web Dashboard điều khiển hoặc đồng bộ người dùng
    if (topicStr == TOPIC_CMD) {
        handleMqttCommand(String(payloadStr));
    }
}

// ==========================================
// XỬ LÝ CÁC LỆNH ĐỒNG BỘ/ĐIỀU KHIỂN TỪ WEB
// ==========================================
void handleMqttCommand(const String& cmdJson) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, cmdJson);
    if (error) {
        Serial.println("[-] Giai ma JSON lenh MQTT that bai!");
        return;
    }

    String cmd = doc["cmd"].as<String>();
    
    // Lệnh điều khiển barrier từ xa
    if (cmd == "open_gate_in") {
        openBarrierIn("lenh remote");
    } 
    else if (cmd == "close_gate_in") {
        closeBarrierIn("lenh remote");
    } 
    else if (cmd == "open_gate_out") {
        openBarrierOut("lenh remote");
    } 
    else if (cmd == "close_gate_out") {
        closeBarrierOut("lenh remote");
    }
    // Lệnh cập nhật/đồng bộ cơ sở dữ liệu thẻ
    else if (cmd == "add_user" || cmd == "update_user") {
        String uid = doc["uid"].as<String>();
        if (uid.length() > 0) {
            User u;
            // Nếu là cập nhật, lấy dữ liệu hiện tại trước để giữ nguyên trạng thái bãi xe/thời gian vào
            bool exists = getUser(uid, u);
            if (!exists) {
                u.uid = uid;
                u.inParking = false;
                u.entryTime = "";
                u.status = "active";
            }
            if (doc.containsKey("name")) u.name = doc["name"].as<String>();
            if (doc.containsKey("studentId")) u.studentId = doc["studentId"].as<String>();
            if (doc.containsKey("plateNumber")) u.plateNumber = doc["plateNumber"].as<String>();
            if (doc.containsKey("vehicleType")) u.vehicleType = doc["vehicleType"].as<String>();
            if (doc.containsKey("balance")) u.balance = doc["balance"].as<long>();
            if (doc.containsKey("status")) u.status = doc["status"].as<String>();
            
            saveUser(u);
            Serial.printf("[+] Dong bo the %s thanh cong.\n", uid.c_str());
        }
    }
    else if (cmd == "update_balance") {
        String uid = doc["uid"].as<String>();
        long balance = doc["balance"].as<long>();
        User u;
        if (getUser(uid, u)) {
            Serial.printf("[SYNC] UID=%s BALANCE_LOCAL_BEFORE_SYNC=%ld BALANCE_SERVER=%ld\n",
                          uid.c_str(), u.balance, balance);
            u.balance = balance;
            saveUser(u);
            Serial.printf("[+] Dong bo so du the %s: %ld VNĐ\n", uid.c_str(), balance);
        }
    }
    else if (cmd == "lock_card") {
        String uid = doc["uid"].as<String>();
        User u;
        if (getUser(uid, u)) {
            u.status = "locked";
            saveUser(u);
            Serial.printf("[+] Khoa the %s thanh cong.\n", uid.c_str());
        }
    }
    else if (cmd == "unlock_card") {
        String uid = doc["uid"].as<String>();
        User u;
        if (getUser(uid, u)) {
            u.status = "active";
            saveUser(u);
            Serial.printf("[+] Mo khoa the %s thanh cong.\n", uid.c_str());
        }
    }
    else if (cmd == "delete_user") {
        String uid = doc["uid"].as<String>();
        if (deleteUser(uid)) {
            Serial.printf("[+] Da xoa the %s khoi bo nho cuc bo.\n", uid.c_str());
        }
    }
}

// ==========================================
// GỬI NHỊP TIM TRẠNG THÁI THIẾT BỊ (HEARTBEAT)
// ==========================================
void sendDeviceStatus() {
    if (!mqttClient.connected()) return;

    StaticJsonDocument<256> doc;
    doc["esp32"] = "online";
    doc["wifi"] = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";
    doc["mqtt"] = mqttClient.connected() ? "connected" : "disconnected";
    doc["pn532"] = (isRfidInOk && isRfidOutOk) ? "connected" : "disconnected";
    doc["rfid_in"] = isRfidInOk ? "connected" : "disconnected";
    doc["rfid_out"] = isRfidOutOk ? "connected" : "disconnected";
    doc["barrier_in"] = barrierInState;
    doc["barrier_out"] = barrierOutState;

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(TOPIC_STATUS, payload.c_str());
}

// ==========================================
// ĐỒNG BỘ LOGS NGOẠI TUYẾN KHI CÓ MẠNG TRỞ LẠI
// ==========================================
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

void handleManualButtons(uint32_t currentMillis) {
    bool inPressed = digitalRead(MANUAL_OPEN_IN_BUTTON_PIN) == LOW;
    if (inPressed) {
        if (manualInPressStart == 0) {
            manualInPressStart = currentMillis;
        } else if (!manualInLongPressHandled &&
                   currentMillis - manualInPressStart >= MANUAL_BUTTON_HOLD_MS) {
            manualInLongPressHandled = true;
            isDisplayActiveMessage = true;
            displayResetTime = currentMillis + 4000;
            showManualOpenScreen("CONG VAO", "Mo tay");
            openBarrierIn("nut nhan thu cong");
            publishParkingEvent("", "manual_open_in", getFormattedTime(getCurrentDateTime()));
        }
    } else {
        manualInPressStart = 0;
        manualInLongPressHandled = false;
    }

    bool outPressed = digitalRead(MANUAL_OPEN_OUT_BUTTON_PIN) == LOW;
    if (outPressed) {
        if (manualOutPressStart == 0) {
            manualOutPressStart = currentMillis;
        } else if (!manualOutLongPressHandled &&
                   currentMillis - manualOutPressStart >= MANUAL_BUTTON_HOLD_MS) {
            manualOutLongPressHandled = true;
            isDisplayActiveMessage = true;
            displayResetTime = currentMillis + 4000;
            showManualOpenScreen("CONG RA", "Tien mat");
            openBarrierOut("nut nhan thu cong");
            publishParkingEvent("", "manual_open_out_cash", getFormattedTime(getCurrentDateTime()));
        }
    } else {
        manualOutPressStart = 0;
        manualOutLongPressHandled = false;
    }
}

void openBarrierIn(const char* source) {
    servoIn.write(BARRIER_OPEN_ANGLE);
    barrierInState = "open";
    barrierInCloseTime = millis() + BARRIER_DELAY_MS;
    Serial.printf("[+] Mo barrier Cong Vao bang %s.\n", source);
    beepSuccess();
    sendDeviceStatus();
}

void closeBarrierIn(const char* source) {
    servoIn.write(BARRIER_CLOSED_ANGLE);
    barrierInState = "closed";
    barrierInCloseTime = 0;
    Serial.printf("[+] Dong barrier CONG VAO bang %s.\n", source);
    sendDeviceStatus();
}

void openBarrierOut(const char* source) {
    servoOut.write(BARRIER_OPEN_ANGLE);
    barrierOutState = "open";
    barrierOutCloseTime = millis() + BARRIER_DELAY_MS;
    Serial.printf("[+] Mo barrier Cong Ra bang %s.\n", source);
    beepSuccess();
    sendDeviceStatus();
}

void closeBarrierOut(const char* source) {
    servoOut.write(BARRIER_CLOSED_ANGLE);
    barrierOutState = "closed";
    barrierOutCloseTime = 0;
    Serial.printf("[+] Dong barrier CONG RA bang %s.\n", source);
    sendDeviceStatus();
}

void publishParkingEvent(const String& uid, const String& event, const String& timeStr, long fee, long balance) {
    StaticJsonDocument<256> doc;
    if (uid.length() > 0) {
        doc["uid"] = uid;
    }
    doc["event"] = event;
    doc["time"] = timeStr;
    if (fee >= 0) {
        doc["fee"] = fee;
    }
    if (balance >= 0) {
        doc["balance"] = balance;
    }

    String payload;
    serializeJson(doc, payload);

    if (mqttClient.connected()) {
        mqttClient.publish(TOPIC_EVENT, payload.c_str());
        return;
    }

    queueOfflineLog(uid, event, fee >= 0 ? fee : 0, balance >= 0 ? balance : 0, timeStr);
}

bool initRc522Reader(MFRC522& reader, const char* gateLabel) {
    resetRc522Bus();
    reader.PCD_Init();
    delay(50);

    byte version = reader.PCD_ReadRegister(MFRC522::VersionReg);
    if (version == 0x00 || version == 0xFF) {
        Serial.printf("[-] Khong tim thay RC522 %s! VersionReg=0x%02X\n", gateLabel, version);
        return false;
    }

    Serial.printf("[+] Da tim thay RC522 %s. VersionReg=0x%02X\n", gateLabel, version);
    reader.PCD_AntennaOn();
    return true;
}

void resetRc522Bus() {
    digitalWrite(RC522_IN_SS, HIGH);
    digitalWrite(RC522_OUT_SS, HIGH);
    digitalWrite(RC522_RST_PIN, LOW);
    delay(20);
    digitalWrite(RC522_RST_PIN, HIGH);
    delay(50);
}

// ==========================================
// THỰC THI QUÉT THẺ TUẦN TỰ TRÊN 2 ĐẦU ĐỌC
// ==========================================
void checkRfidGates() {
    uint8_t uidLength;
    
    // Dùng chung bus SPI. MFRC522 sẽ tự điều khiển chân SS theo từng đầu đọc.
    
    // --- 1. Quét đầu đọc CỔNG VÀO (IN) ---
    if (isRfidInOk) {
        if (nfcIn.PICC_IsNewCardPresent() && nfcIn.PICC_ReadCardSerial()) {
            uidLength = nfcIn.uid.size;
            String uidStr = "";
            for (uint8_t i = 0; i < uidLength; i++) {
                byte value = nfcIn.uid.uidByte[i];
                if (value < 0x10) uidStr += "0";
                uidStr += String(value, HEX);
            }
            uidStr.toUpperCase();
            
            // Chống nhiễu rung đầu đọc (Debounce)
            uint32_t now = millis();
            if (uidStr != lastScannedUid || (now - lastScanTime > DEBOUNCE_TIME_MS)) {
                lastScannedUid = uidStr;
                lastScanTime = now;
                Serial.printf("\n[Card] Cong VAO phat hien the UID: %s\n", uidStr.c_str());
                processCheckIn(uidStr);
            }
            nfcIn.PICC_HaltA();
            nfcIn.PCD_StopCrypto1();
            return; // Trả về để tránh xung đột bus SPI ngay lập tức
        }
    }

    // --- 2. Quét đầu đọc CỔNG RA (OUT) ---
    if (isRfidOutOk) {
        if (nfcOut.PICC_IsNewCardPresent() && nfcOut.PICC_ReadCardSerial()) {
            uidLength = nfcOut.uid.size;
            String uidStr = "";
            for (uint8_t i = 0; i < uidLength; i++) {
                byte value = nfcOut.uid.uidByte[i];
                if (value < 0x10) uidStr += "0";
                uidStr += String(value, HEX);
            }
            uidStr.toUpperCase();
            
            // Chống nhiễu rung đầu đọc (Debounce)
            uint32_t now = millis();
            if (uidStr != lastScannedUid || (now - lastScanTime > DEBOUNCE_TIME_MS)) {
                lastScannedUid = uidStr;
                lastScanTime = now;
                Serial.printf("\n[Card] Cong RA phat hien the UID: %s\n", uidStr.c_str());
                processCheckOut(uidStr);
            }
            nfcOut.PICC_HaltA();
            nfcOut.PCD_StopCrypto1();
            return;
        }
    }
}

// ==========================================
// XỬ LÝ XE VÀO BÃI (CHECK-IN)
// ==========================================
void processCheckIn(const String& uid) {
    User user;
    isDisplayActiveMessage = true;
    displayResetTime = millis() + 4000; // Hiển thị kết quả trong 4 giây

    // 1. Kiểm tra thẻ trong Database cục bộ
    if (!getUser(uid, user)) {
        showWarningScreen("THE CHUA DANG KY!");
        beepError();
        
        // Gửi thông báo thẻ không xác định lên MQTT
        if (mqttClient.connected()) {
            mqttClient.publish(TOPIC_SCAN, uid.c_str());
        }
        return;
    }

    // 2. Kiểm tra thẻ có bị khóa hay không
    if (user.status == "locked") {
        showWarningScreen("THE NAY DA BI KHOA!");
        beepError();
        return;
    }

    // 3. Kiểm tra xe có đang nằm trong bãi không
    if (user.inParking) {
        showWarningScreen("XE DANG TRONG BAI!");
        beepError();
        return;
    }

    // 4. Lấy thời gian từ RTC để cập nhật trạng thái vào bãi
    DateTime nowDt = getCurrentDateTime();
    String timeStr = getFormattedTime(nowDt);
    Serial.printf("[CHECKIN] UID=%s ENTRY_TIME=%s BALANCE_BEFORE=%ld\n",
                  uid.c_str(), timeStr.c_str(), user.balance);

    // Cập nhật thông tin cục bộ
    user.inParking = true;
    user.entryTime = timeStr;
    saveUser(user);

    // 5. Hiển thị thông tin thành công lên màn hình TFT và phát còi bíp
    showCheckInSuccess(user.name, user.plateNumber, timeStr);

    // 6. Điều khiển mở barrier cổng vào
    openBarrierIn("quet the hop le");

    // 7. Đồng bộ dữ liệu sự kiện lên MQTT
    publishParkingEvent(uid, "checkin", timeStr);
}

// ==========================================
// XỬ LÝ XE RA KHỎI BÃI (CHECK-OUT)
// ==========================================
void processCheckOut(const String& uid) {
    User user;
    isDisplayActiveMessage = true;
    displayResetTime = millis() + 4000;

    // 1. Kiểm tra thẻ trong DB cục bộ
    if (!getUser(uid, user)) {
        showWarningScreen("THE CHUA DANG KY!");
        beepError();
        
        if (mqttClient.connected()) {
            mqttClient.publish(TOPIC_SCAN, uid.c_str());
        }
        return;
    }

    // 2. Kiểm tra thẻ bị khóa
    if (user.status == "locked") {
        showWarningScreen("THE NAY DA BI KHOA!");
        beepError();
        return;
    }

    // 3. Kiểm tra xe có ở trong bãi không
    if (!user.inParking) {
        showWarningScreen("XE CHUA VAO BAI!");
        beepError();
        return;
    }

    // 4. Lấy thời gian hiện tại từ RTC để tính toán phí gửi
    DateTime nowDt = getCurrentDateTime();
    String exitTimeStr = getFormattedTime(nowDt);

    long fee = calculateParkingFee(user.entryTime, nowDt);
    Serial.printf("[CHECKOUT] UID=%s ENTRY_TIME=%s EXIT_TIME=%s LOCAL_FEE=%ld BALANCE_BEFORE=%ld\n",
                  uid.c_str(),
                  user.entryTime.c_str(),
                  exitTimeStr.c_str(),
                  fee,
                  user.balance);

    // 5. Kiểm tra số dư tài khoản
    if (user.balance < fee) {
        char errBuf[32];
        snprintf(errBuf, sizeof(errBuf), "KHONG DU TIEN!\nPhi: %ld d\nDu: %ld d", fee, user.balance);
        showWarningScreen(String(errBuf));
        beepWarning();
        return;
    }

    // 6. Trừ tiền và cập nhật trạng thái ra bãi
    user.balance -= fee;
    user.inParking = false;
    user.entryTime = ""; // Reset thời gian vào
    saveUser(user);
    Serial.printf("[CHECKOUT] UID=%s BALANCE_AFTER_LOCAL=%ld\n",
                  uid.c_str(), user.balance);

    // 7. Hiển thị thông tin xe ra lên TFT và còi bíp thành công
    showCheckOutSuccess(user.name, user.plateNumber, fee, user.balance);

    // 8. Điều khiển mở barrier cổng ra
    openBarrierOut("quet the hop le");

    // 9. Đồng bộ dữ liệu sự kiện lên MQTT
    publishParkingEvent(uid, "checkout", exitTimeStr, fee, user.balance);
}

// ==========================================
// TÍNH PHÍ GỬI XE DỰA TRÊN THỜI GIAN RTC
// ==========================================
long calculateParkingFee(const String& entryTimeStr, const DateTime& exitTime) {
    int ey, em, ed, eh, emin;
    if (!parseDateTime(entryTimeStr, ey, em, ed, eh, emin)) {
        // Nếu phân tích thời gian lỗi, áp dụng phí cơ bản (Daytime) làm mặc định
        return FEE_DAYTIME;
    }

    // 1. Kiểm tra gửi qua đêm (Khác ngày)
    if (ey != exitTime.year() || em != exitTime.month() || ed != exitTime.day()) {
        return FEE_OVERNIGHT; // 11.000 VNĐ
    }

    // 2. Cùng ngày gửi
    // Xe ra sau 18:00
    if (exitTime.hour() >= 18) {
        return FEE_NIGHTTIME; // 5.000 VNĐ
    } 
    // Xe vào và ra trong khoảng 07:00 -> 18:00
    else {
        return FEE_DAYTIME; // 3.000 VNĐ
    }
}

// ==========================================
// HÀM PHỤ TRỢ PARSE VÀ FORMAT THỜI GIAN
// ==========================================
bool parseDateTime(const String& str, int& y, int& m, int& d, int& h, int& min) {
    // Định dạng mong muốn: "YYYY-MM-DD HH:MM:SS"
    if (str.length() < 16) return false;
    y = str.substring(0, 4).toInt();
    m = str.substring(5, 7).toInt();
    d = str.substring(8, 10).toInt();
    h = str.substring(11, 13).toInt();
    min = str.substring(14, 16).toInt();
    return true;
}

String getFormattedTime(const DateTime& dt) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             dt.year(), dt.month(), dt.day(),
             dt.hour(), dt.minute(), dt.second());
    return String(buf);
}

DateTime getCurrentDateTime() {
    if (isRtcOk) {
        return rtc.now();
    }

    uint32_t elapsedSeconds = (millis() - rtcFallbackStartMillis) / 1000;
    return rtcFallbackBase + TimeSpan(elapsedSeconds);
}
