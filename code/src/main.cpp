#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <LittleFS.h>          // De dem so xe trong bai o idle screen

#include "Config.h"
#include "Database.h"
#include "Buzzer.h"
#include "ParkingDisplay.h"
#include "TimeManager.h"        // Cung cap DateTime, getCurrentDateTime, ...
#include "BarrierController.h"
#include "RfidGates.h"
#include "WifiMqttClient.h"
#include "ParkingFlow.h"

// ==========================================
// ĐỐI TƯỢNG VÀ BIẾN TOÀN CỤC
// ==========================================
// WiFi/MQTT/heartbeat -> WifiMqttClient
// RC522 -> RfidGates
// Servo -> BarrierController
// Cash + display busy + manual guard -> ParkingFlow

// State debounce nut bam tu vat ly (chi dung trong handleManualButtons)
bool     manualInStableState       = HIGH;
bool     manualOutStableState      = HIGH;
bool     manualInLastReading       = HIGH;
bool     manualOutLastReading      = HIGH;
uint32_t manualInLastDebounceTime  = 0;
uint32_t manualOutLastDebounceTime = 0;

// ==========================================
// KHAI BÁO CÁC HÀM CON
// ==========================================
void handleManualButtons(uint32_t currentMillis);

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

    // 4. Khởi tạo RTC DS3231 (qua TimeManager)
    showBootingScreen("3. RTC DS3231 Init...", 50);
    initTimeManager();
    delay(500);

    // 5. Khởi tạo Servo Barrier (qua BarrierController)
    showBootingScreen("4. Servo Init...", 70);
    initBarriers();
    delay(500);

    // 6. Khởi tạo các đầu đọc RFID RC522 (qua RfidGates)
    showBootingScreen("5. RFID RC522 Init...", 85);
    initRfidGates(processCheckIn, processCheckOut);
    delay(500);

    // 7. Kết nối WiFi + cấu hình MQTT broker (qua WifiMqttClient)
    showBootingScreen("6. Connecting WiFi...", 95);
    initWifiMqtt();

    // 7.1. Đồng bộ giờ qua NTP nếu có Internet (chuẩn hơn nhiều so với compile time)
    if (isWifiConnected()) {
        showBootingScreen("7. Sync NTP time...", 97);
        syncRtcFromNtp(8000);
    }

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

    // 1. Duy trì kết nối mạng + đồng bộ giờ
    maintainWifi(currentMillis);
    maintainNtpSync(currentMillis);

    // 2. Duy trì kết nối MQTT Broker + đẩy log offline
    maintainMqtt(currentMillis);

    // 3. Heartbeat trạng thái thiết bị mỗi 5 giây
    maintainHeartbeat(currentMillis);

    // 4. Tự động đóng cổng sau khi mở quá thời gian delay
    maintainBarriers(currentMillis);

    // 4.1. Hết thời gian chờ thanh toán tiền mặt -> huỷ phiên
    maintainCashCheckout(currentMillis);

    // 5. & 6. Cập nhật giao diện màn hình Chờ (Idle Screen) khi không có thông báo
    if (!isDisplayBusy(currentMillis)) {
        DateTime nowDt = getCurrentDateTime();
        String dtStr = getFormattedTime(nowDt);
        
        // Đếm số lượng xe trong bãi
        int parkedCount = 0;
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file) {
            String filename = file.name();
            String userFilePath = filename;
            if (!userFilePath.startsWith("/")) {
                userFilePath = "/" + userFilePath;
            }

            if (userFilePath.startsWith("/u_") && userFilePath.endsWith(".json")) {
                file.close();
                User testUser;
                String uidFromFile = userFilePath.substring(3, userFilePath.length() - 5);
                if (getUser(uidFromFile, testUser) && testUser.inParking) {
                    parkedCount++;
                }
            } else {
                file.close();
            }
            file = root.openNextFile();
        }
        
        showIdleScreen(dtStr, parkedCount, isWifiConnected(), isMqttConnected());
    }

    // 7. Quét các thẻ NFC tại 2 đầu đọc cổng vào/ra
    handleManualButtons(currentMillis);
    pollRfidGates();

    // Delay ngắn để tránh quá nhiệt và nhường quyền xử lý luồng WiFi
    delay(10);
}

// Cac ham WiFi/MQTT/heartbeat/offline queue da chuyen sang WifiMqttClient.cpp

void handleManualButtons(uint32_t currentMillis) {
    // ----- Nut CONG VAO -----
    bool inReading = digitalRead(MANUAL_OPEN_IN_BUTTON_PIN);
    if (inReading != manualInLastReading) {
        manualInLastDebounceTime = currentMillis;
        manualInLastReading      = inReading;
    }
    if (currentMillis - manualInLastDebounceTime >= MANUAL_BUTTON_DEBOUNCE_MS &&
        inReading != manualInStableState) {
        manualInStableState = inReading;
        if (manualInStableState == LOW) {
            markDisplayBusyFor(4000);
            if (isBarrierInOpen()) {
                showManualBarrierScreen("CONG VAO", false, "Nut nhan");
                closeBarrierIn("nut nhan thu cong");
                publishParkingEvent("", "manual_close_in", getFormattedTime(getCurrentDateTime()));
            } else {
                showManualBarrierScreen("CONG VAO", true, "Nut nhan");
                openBarrierIn("nut nhan thu cong");
                publishParkingEvent("", "manual_open_in", getFormattedTime(getCurrentDateTime()));
            }
        }
    }

    // ----- Nut CONG RA -----
    bool outReading = digitalRead(MANUAL_OPEN_OUT_BUTTON_PIN);
    if (outReading != manualOutLastReading) {
        manualOutLastDebounceTime = currentMillis;
        manualOutLastReading      = outReading;
    }
    if (currentMillis - manualOutLastDebounceTime >= MANUAL_BUTTON_DEBOUNCE_MS &&
        outReading != manualOutStableState) {
        manualOutStableState = outReading;
        if (manualOutStableState != LOW) return;

        if (isWithinManualOutGuard(currentMillis)) {
            return;
        }
        // Trong cua so cho thanh toan tien mat -> xac nhan da thu tien
        if (isCashCheckoutPending()) {
            confirmCashCheckout();
            return;
        }
        // Het cua so cho tien mat nhung con co trang thai cu -> dam bao xoa
        clearCashCheckout();

        markDisplayBusyFor(4000);
        setManualOutGuard(currentMillis + MANUAL_OUT_BUTTON_GUARD_MS);

        if (isBarrierOutOpen()) {
            showManualBarrierScreen("CONG RA", false, "Nut nhan");
            closeBarrierOut("nut nhan thu cong");
            publishParkingEvent("", "manual_close_out", getFormattedTime(getCurrentDateTime()));
        } else {
            showManualBarrierScreen("CONG RA", true, "Nut nhan");
            openBarrierOut("nut nhan thu cong");
            publishParkingEvent("", "manual_open_out", getFormattedTime(getCurrentDateTime()));
        }
    }
}

// Toan bo nghiep vu check-in/out + cash + tinh phi -> ParkingFlow.cpp
// Open/close barrier  -> BarrierController.cpp
// publishParkingEvent -> WifiMqttClient.cpp
// RC522 quet the      -> RfidGates.cpp
// RTC / NTP           -> TimeManager.cpp
