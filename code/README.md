# Hướng dẫn nạp chương trình và kết nối phần cứng ESP32-S3 N16R8

Thư mục này chứa mã nguồn (firmware) C/C++ lập trình trên vi điều khiển **ESP32-S3 DevKitC-1 (bản N16R8 - 16MB Flash, 8MB PSRAM)** cho mô hình bãi gửi xe thông minh.

## 1. Sơ đồ đấu nối dây phần cứng (Wiring Diagram)

Hãy kết nối các linh kiện ngoại vi với ESP32-S3 theo đúng sơ đồ chân dưới đây:

### 1.1 Đầu đọc thẻ NFC PN532 (Giao tiếp SPI chung bus)
*Hai module PN532 dùng chung các đường SCK, MISO, MOSI của cổng SPI phần cứng và phân biệt bằng chân SS (Chip Select) riêng.*

| Ký hiệu chân PN532 | Chân kết nối ESP32-S3 | Ghi chú |
| :--- | :--- | :--- |
| **VCC** | 3.3V | Cần cấp nguồn 3.3V ổn định |
| **GND** | GND | Mass chung |
| **SCK** | **GPIO12** | Chân clock chung SPI |
| **MISO** | **GPIO13** | Chân Master In Slave Out chung SPI |
| **MOSI** | **GPIO11** | Chân Master Out Slave In chung SPI |
| **SS (Chip Select Vào)** | **GPIO10** | Chân chọn đầu đọc Cổng Vào |
| **SS (Chip Select Ra)** | **GPIO14** | Chân chọn đầu đọc Cổng Ra |

*Chú ý: Nhớ gạt Switch (nếu có) trên module PN532 sang chế độ truyền thông SPI (thường là gạt nút 1: ON, nút 2: OFF).*

### 1.2 Màn hình TFT ST7735 1.8" (SPI)
*Màn hình dùng chung các chân xung và dữ liệu SPI với đầu đọc PN532.*

| Ký hiệu chân TFT | Chân kết nối ESP32-S3 | Ghi chú |
| :--- | :--- | :--- |
| **VCC** | 3.3V | Nguồn nuôi mạch |
| **GND** | GND | Mass chung |
| **SCL (SCK)** | **GPIO12** | Dùng chung chân SCK |
| **SDA (MOSI)** | **GPIO11** | Dùng chung chân MOSI |
| **RES (RST)** | **GPIO4** | Chân Reset màn hình |
| **DC (RS)** | **GPIO2** | Chân lệnh/dữ liệu |
| **CS** | **GPIO15** | Chân chọn màn hình TFT |
| **BL (LED)** | 3.3V | Cấp nguồn cho đèn nền màn hình (luôn sáng) |

### 1.3 Cảm biến thời gian thực RTC DS3231 (I2C)
*Giao tiếp qua bus I2C.*

| Ký hiệu chân RTC | Chân kết nối ESP32-S3 | Ghi chú |
| :--- | :--- | :--- |
| **VCC** | 3.3V | Nguồn nuôi mạch |
| **GND** | GND | Mass chung |
| **SDA** | **GPIO8** | Giao tiếp dữ liệu I2C |
| **SCL** | **GPIO9** | Giao tiếp xung clock I2C |

### 1.4 Servo điều khiển Barrier & Còi Buzzer
*Lưu ý: Mạch servo cần dòng điện lớn, nên cấp nguồn 5V ngoài thay vì lấy trực tiếp từ chip ESP32-S3 để tránh sụt áp gây reset vi điều khiển.*

| Thiết bị | Chân thiết bị | Chân kết nối ESP32-S3 | Ghi chú |
| :--- | :--- | :--- | :--- |
| **Servo Cổng Vào** | Signal (Dây cam) | **GPIO16** | Điều khiển mở barrier vào |
| **Servo Cổng Ra** | Signal (Dây cam) | **GPIO17** | Điều khiển mở barrier ra (tránh PSRAM) |
| **Buzzer** | Chân dương (+) | **GPIO18** | Còi phát tín hiệu âm thanh (tránh PSRAM) |
| **Nút tay Cổng Vào** | Một chân nút | **GPIO6** | Nhấn một lần để đóng/mở servo cổng vào |
| **Nút tay Cổng Ra** | Một chân nút | **GPIO7** | Nhấn một lần để đóng/mở servo cổng ra |

*Đấu hai nút nhấn theo kiểu một đầu vào GPIO, đầu còn lại xuống **GND**. Firmware dùng `INPUT_PULLUP`, nên trạng thái nhấn là mức LOW.*

### 1.5 Thanh toán tiền mặt khi không đủ số dư

- Firmware không cần phân loại thẻ khách hay thẻ sinh viên để xử lý cổng ra.
- Với mọi thẻ hợp lệ, khi quét ở cổng ra hệ thống sẽ tính phí như bình thường.
- Nếu **đủ số dư**: trừ tiền trong thẻ, cho xe ra và đồng bộ web.
- Nếu **không đủ số dư**: hệ thống chuyển sang trạng thái chờ tối đa **30 giây**.
- Trong 30 giây đó, nhấn nút `Cổng Ra` sẽ xác nhận **đã thu tiền mặt**, cho xe ra bãi và đồng bộ web.
- Nếu quá 30 giây không bấm nút, giao dịch tiền mặt bị hủy; thẻ vẫn chưa được cho ra bãi.
- MQTT event cho web:
  - `checkout_wait_cash`: trạng thái trung gian, đang chờ thu tiền mặt
  - `checkout`: xe đã ra bãi thành công, web cần xóa khỏi danh sách đang đỗ

---

## 2. Thư viện phụ thuộc (Library Dependencies)

PlatformIO sẽ tự động tải xuống các thư viện dưới đây khi bạn mở dự án:
1. **Adafruit_PN532** (tác giả Adafruit) - Thư viện giao tiếp đầu đọc NFC.
2. **Adafruit_GFX_Library** (tác giả Adafruit) - Thư viện lõi đồ họa.
3. **Adafruit_ST7735_and_ST7789_Library** (tác giả Adafruit) - Thư viện driver màn hình.
4. **RTClib** (tác giả Adafruit) - Thư viện giao tiếp RTC DS3231.
5. **ESP32Servo** (tác giả John K. Bennett) - Thư viện điều khiển servo chuẩn cho ESP32.
6. **PubSubClient** (tác giả Nick O'Leary) - Thư viện truyền thông giao thức MQTT.
7. **ArduinoJson** (tác giả Benoit Blanchon) - Thư viện xử lý/phân tích chuỗi dữ liệu JSON (yêu cầu bản **v6.x**).

---

## 3. Cấu hình phần mềm trước khi nạp

Mở tệp tin [Config.h](file:///d:/NVGN/smart-parking-iot-dashboard/code/include/Config.h) và tiến hành cấu hình lại thông số mạng phù hợp với bãi xe thực tế của bạn:

```cpp
// Thay đổi thông tin mạng WiFi của bạn
#define WIFI_SSID         "Tên_WiFi_Nhà_Bạn"
#define WIFI_PASSWORD     "Mật_Khẩu_WiFi"

// Thay đổi MQTT Broker (nếu sử dụng máy chủ riêng hoặc local broker)
#define MQTT_BROKER       "broker.hivemq.com"
#define MQTT_PORT         1883
#define MQTT_USER         ""
#define MQTT_PASS         ""

// Tiền tố topic đồng bộ (cần khớp với cấu hình phía Node.js Backend)
#define TOPIC_PREFIX      "parking/system"
```

---

