#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// CẤU HÌNH KẾT NỐI WIFI & MQTT
// ==========================================
#define WIFI_SSID         "IoT LAB"
#define WIFI_PASSWORD     "kvt1ptit"

// Sử dụng Broker mặc định HiveMQ. Có thể đổi sang MQTT Broker khác nếu cần.
#define MQTT_BROKER       "broker.hivemq.com"
#define MQTT_PORT         1883
#define MQTT_USER         "parking_admin" // Để trống nếu không dùng username
#define MQTT_PASS         "ADmin321" // Để trống nếu không dùng password

// Tiền tố topic MQTT đồng bộ dữ liệu
#define TOPIC_PREFIX      "parking/system"

// Các topic MQTT cụ thể
#define TOPIC_SCAN        "parking/system/scan"
#define TOPIC_EVENT       "parking/system/event"
#define TOPIC_STATUS      "parking/system/device/status"
#define TOPIC_CMD         "parking/system/cmd"

// ==========================================
// ĐỊNH NGHĨA SƠ ĐỒ CHÂN PHẦN CỨNG (PINOUT CHO ESP32-S3 N16R8)
// ==========================================

// 1. Bus SPI dùng chung cho RC522 và TFT ST7735 (Sử dụng chân phần cứng mặc định của S3)
#define SPI_SCK_PIN       12
#define SPI_MISO_PIN      13
#define SPI_MOSI_PIN      11

// 2. Chân chọn thiết bị SPI (Chip Select - SS)
#define RC522_IN_SS       10  // Đầu đọc cổng vào
#define RC522_OUT_SS      14  // Đầu đọc cổng ra
#define RC522_RST_PIN     5   // Chân reset dùng chung cho 2 RC522
#define TFT_CS            15  // Chip Select màn hình TFT

// 3. Các chân điều khiển khác của màn hình TFT ST7735
#define TFT_RST           4   // Chân Reset (RES)
#define TFT_DC            2   // Chân Data/Command Select

// 4. Giao tiếp I2C cho RTC DS3231 (Sử dụng chân I2C an toàn trên S3)
#define I2C_SDA           8
#define I2C_SCL           9

// 5. Chân điều khiển Servo Barrier (Tránh chân GPIO26 bị xung đột với PSRAM)
#define SERVO_IN_PIN      16  // Cổng vào
#define SERVO_OUT_PIN     17  // Cổng ra

// Cấu hình góc xoay Servo Barrier
#define BARRIER_CLOSED_ANGLE 0
#define BARRIER_OPEN_ANGLE   90
#define BARRIER_DELAY_MS     3000 // Thời gian chờ xe đi qua trước khi đóng cổng

// 6. Chân còi cảnh báo Buzzer (Tránh chân GPIO27 bị xung đột với PSRAM)
#define BUZZER_PIN        18

#define BUZZER_ACTIVE_HIGH 1

// 7. Nút nhấn mở cổng thủ công
#define MANUAL_OPEN_IN_BUTTON_PIN   6
#define MANUAL_OPEN_OUT_BUTTON_PIN  7
#define MANUAL_BUTTON_DEBOUNCE_MS   80
#define MANUAL_OUT_BUTTON_GUARD_MS  800
#define CASH_CONFIRM_MS             30000

// ==========================================
// CẤU HÌNH PHÍ GỬI XE (VNĐ)
// ==========================================
#define FEE_DAYTIME       3000   // Cùng ngày, ra trước 18:00 (hoặc trong khoảng 07:00 - 18:00)
#define FEE_NIGHTTIME     5000   // Cùng ngày, ra sau 18:00
#define FEE_OVERNIGHT     11000  // Khác ngày gửi (qua đêm)

#define FEE_NEXT_DAY_FREE_UNTIL_HOUR 6 // Qua ngay, ra truoc gio nay thi khong cong phi ngay cuoi

#endif // CONFIG_H
