#include "ParkingDisplay.h"
void drawSectionBanner(const String& title, uint16_t fillColor, uint16_t textColor = ST77XX_WHITE);
void drawHeader(uint16_t headerBgColor, const String& title, uint16_t titleTextColor = ST77XX_WHITE);

// Khởi tạo đối tượng màn hình TFT dùng chung hardware SPI với RC522.
// Thứ tự tham số: CS, DC, RST
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Bảng màu thiết kế giao diện
#define COLOR_BACKGROUND  ST77XX_WHITE
#define COLOR_TEXT        ST77XX_BLACK
#define COLOR_HEADER      0x39E7 // Màu xám xanh tối
#define COLOR_GREEN       ST77XX_GREEN
#define COLOR_BLUE        0x03FF // Màu xanh dương nhạt (Cyan-ish)
#define COLOR_RED         ST77XX_RED
#define COLOR_ORANGE      0xFD20
#define COLOR_YELLOW      ST77XX_YELLOW
#define COLOR_PANEL       0xEF5D

namespace {
constexpr int16_t SCREEN_WIDTH = 160;
constexpr int16_t SCREEN_HEIGHT = 128;
constexpr int16_t HEADER_LINE_Y = 29;
constexpr int16_t CONTENT_TOP_Y = HEADER_LINE_Y + 6;
constexpr int16_t FOOTER_Y = 112;

enum ScreenMode {
    SCREEN_NONE,
    SCREEN_BOOT,
    SCREEN_IDLE,
    SCREEN_CHECKIN,
    SCREEN_CHECKOUT,
    SCREEN_MANUAL_OPEN,
    SCREEN_WARNING
};

ScreenMode currentScreenMode = SCREEN_NONE;

void invalidateIdleCache() {
    currentScreenMode = SCREEN_NONE;
}

void drawTextCentered(const String& text, int16_t y, uint16_t color, uint8_t size = 1) {
    int16_t textWidth = text.length() * 6 * size;
    int16_t x = (SCREEN_WIDTH - textWidth) / 2;
    if (x < 0) x = 0;
    tft.setTextColor(color);
    tft.setTextSize(size);
    tft.setCursor(x, y);
    tft.print(text);
}

void drawMultilineText(const String& text, int16_t x, int16_t y, uint16_t color) {
    tft.setTextColor(color);
    tft.setTextSize(1);

    int16_t lineStart = 0;
    int16_t currentY = y;

    while (lineStart <= text.length()) {
        int16_t lineEnd = text.indexOf('\n', lineStart);
        if (lineEnd < 0) lineEnd = text.length();

        tft.setCursor(x, currentY);
        tft.print(text.substring(lineStart, lineEnd));

        currentY += 12;
        if (lineEnd == text.length()) break;
        lineStart = lineEnd + 1;
    }
}

void drawIdleStaticLayout() {
    drawHeader(COLOR_HEADER, "PARKING");
    drawSectionBanner("SAN SANG QUET THE", COLOR_HEADER);

    tft.fillRoundRect(8, 54, 66, 42, 6, COLOR_PANEL);
    tft.drawRoundRect(8, 54, 66, 42, 6, COLOR_HEADER);
    tft.setCursor(16, 63);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HEADER);
    tft.print("XE TRONG BAI");

    tft.fillRoundRect(84, 54, 68, 42, 6, COLOR_PANEL);
    tft.drawRoundRect(84, 54, 68, 42, 6, COLOR_HEADER);
    tft.setCursor(96, 63);
    tft.setTextColor(COLOR_HEADER);
    tft.setTextSize(1);
    tft.print("KET NOI");

    tft.drawRoundRect(8, 100, 144, 10, 4, COLOR_BLUE);
    tft.fillRoundRect(12, 103, 136, 4, 2, COLOR_BLUE);
    tft.setCursor(24, 88);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BLUE);
    tft.print("VUI LONG QUET THE !");

    tft.fillRect(0, FOOTER_Y, SCREEN_WIDTH, SCREEN_HEIGHT - FOOTER_Y, COLOR_HEADER);
}

void drawIdleCount(int parkedCount) {
    tft.fillRect(18, 76, 48, 18, COLOR_PANEL);
    tft.setCursor(24, 79);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_GREEN);
    if (parkedCount < 10) tft.print("0");
    tft.print(parkedCount);
}

void drawIdleConnection(bool wifiOk, bool mqttOk) {
    tft.fillRect(90, 76, 56, 10, COLOR_PANEL);
    tft.setCursor(92, 78);
    tft.setTextSize(1);
    tft.setTextColor(wifiOk ? COLOR_GREEN : COLOR_RED);
    tft.print("WiFi");
    tft.setTextColor(COLOR_TEXT);
    tft.print("/");
    tft.setTextColor(mqttOk ? COLOR_GREEN : COLOR_RED);
    tft.print("MQTT");
}

void drawIdleFooter(const String& displayTime, bool wifiOk, bool mqttOk) {
    tft.fillRect(0, FOOTER_Y, SCREEN_WIDTH, SCREEN_HEIGHT - FOOTER_Y, COLOR_HEADER);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(5, 116);
    tft.print(displayTime);

    tft.setCursor(120, 116);
    tft.setTextColor(wifiOk ? COLOR_GREEN : COLOR_RED);
    tft.print("W");
    tft.setTextColor(ST77XX_WHITE);
    tft.print("/");
    tft.setTextColor(mqttOk ? COLOR_GREEN : COLOR_RED);
    tft.print("M");
}
}

// Banner tiêu đề phụ cho từng màn hình
void drawSectionBanner(const String& title, uint16_t fillColor, uint16_t textColor) {
    tft.fillRoundRect(6, CONTENT_TOP_Y, 148, 16, 4, fillColor);
    drawTextCentered(title, CONTENT_TOP_Y + 4, textColor, 1);
}

// Header dùng chung cho tất cả các màn hình, theo layout mẫu
void drawHeader(uint16_t headerBgColor, const String& title, uint16_t titleTextColor) {
    (void)headerBgColor;
    (void)title;
    (void)titleTextColor;

    tft.fillScreen(COLOR_BACKGROUND);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_BLACK);
    drawTextCentered("Smart Parking", 12, ST77XX_BLACK, 1);

    tft.drawFastHLine(0, HEADER_LINE_Y, SCREEN_WIDTH, ST77XX_BLACK);
    tft.drawFastHLine(0, HEADER_LINE_Y + 1, SCREEN_WIDTH, ST77XX_BLACK);
}

void initDisplay() {
    // Khởi tạo màn hình ST7735S chip đen (thông dụng) trên shared hardware SPI
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1); // Xoay ngang (160x128)
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextWrap(true);
}

void showBootingScreen(const String& step, int progressPercent) {
    invalidateIdleCache();
    currentScreenMode = SCREEN_BOOT;
    drawHeader(COLOR_HEADER, "BOOTING");
    drawSectionBanner("KHOI DONG", COLOR_HEADER);

    tft.setTextColor(COLOR_BLUE);
    tft.setTextSize(1);
    drawTextCentered("SMART PARKING", 58, COLOR_BLUE, 1);
    
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(26, 78);
    tft.print("System Initializing...");

    int barWidth = 120;
    int barHeight = 8;
    int barX = 20;
    int barY = 94;
    
    tft.drawRect(barX, barY, barWidth, barHeight, COLOR_TEXT);
    int progressWidth = (barWidth - 4) * progressPercent / 100;
    tft.fillRect(barX + 2, barY + 2, progressWidth, barHeight - 4, COLOR_BLUE);
    
    tft.setCursor(10, 108);
    tft.setTextColor(COLOR_TEXT);
    tft.print(step);
}

void showIdleScreen(const String& dateTimeStr, int parkedCount, bool wifiOk, bool mqttOk) {
    static int lastParkedCount = -1;
    static bool lastWifiOk = false;
    static bool lastMqttOk = false;
    static String lastDisplayTime = "";

    String displayTime = dateTimeStr;
    if (dateTimeStr.length() >= 16) {
        displayTime = dateTimeStr.substring(2, 16);
    }

    bool connectionChanged = (wifiOk != lastWifiOk) || (mqttOk != lastMqttOk);
    bool timeChanged = displayTime != lastDisplayTime;

    if (currentScreenMode != SCREEN_IDLE) {
        drawIdleStaticLayout();
        lastParkedCount = -1;
        lastWifiOk = !wifiOk;
        lastMqttOk = !mqttOk;
        lastDisplayTime = "";
        currentScreenMode = SCREEN_IDLE;
        connectionChanged = true;
        timeChanged = true;
    }

    if (parkedCount != lastParkedCount) {
        drawIdleCount(parkedCount);
        lastParkedCount = parkedCount;
    }

    if (connectionChanged) {
        drawIdleConnection(wifiOk, mqttOk);
        lastWifiOk = wifiOk;
        lastMqttOk = mqttOk;
    }

    if (timeChanged || connectionChanged) {
        drawIdleFooter(displayTime, wifiOk, mqttOk);
        lastDisplayTime = displayTime;
    }
}

void showCheckInSuccess(const String& name, const String& plate, const String& entryTime) {
    invalidateIdleCache();
    currentScreenMode = SCREEN_CHECKIN;
    drawHeader(COLOR_GREEN, "CHECK-IN OK", ST77XX_BLACK);
    drawSectionBanner("CHECK-IN OK", COLOR_GREEN, ST77XX_BLACK);

    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(1);
    
    tft.setCursor(10, 56);
    tft.print("Chu xe: ");
    tft.setTextColor(COLOR_YELLOW);
    tft.print(name);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(10, 74);
    tft.print("Bien so: ");
    tft.setTextColor(COLOR_YELLOW);
    tft.print(plate);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(10, 92);
    tft.print("Gio vao: ");
    tft.setTextColor(COLOR_BLUE);
    
    // Trích xuất giờ từ chuỗi YYYY-MM-DD HH:MM:SS
    String timeOnly = entryTime;
    if (entryTime.length() > 11) {
        timeOnly = entryTime.substring(11); // Lấy phần HH:MM:SS
    }
    tft.print(timeOnly);

    tft.fillRect(0, FOOTER_Y, SCREEN_WIDTH, SCREEN_HEIGHT - FOOTER_Y, COLOR_GREEN);
    tft.setTextColor(ST77XX_BLACK);
    tft.setCursor(35, 113);
    tft.print("MO CONG VAO...");
}

void showCheckOutSuccess(const String& name, const String& plate, long fee, long balance) {
    invalidateIdleCache();
    currentScreenMode = SCREEN_CHECKOUT;
    drawHeader(COLOR_BLUE, "CHECK-OUT OK", ST77XX_BLACK);
    drawSectionBanner("CHECK-OUT OK", COLOR_BLUE, ST77XX_BLACK);

    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(1);
    
    tft.setCursor(10, 52);
    tft.print("Chu xe: ");
    tft.setTextColor(COLOR_YELLOW);
    tft.print(name);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(10, 68);
    tft.print("Bien so: ");
    tft.setTextColor(COLOR_YELLOW);
    tft.print(plate);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(10, 84);
    tft.print("Phi gui: ");
    tft.setTextColor(COLOR_RED);
    tft.print(fee);
    tft.print(" d");

    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(10, 100);
    tft.print("So du: ");
    tft.setTextColor(COLOR_GREEN);
    tft.print(balance);
    tft.print(" d");

    tft.fillRect(0, FOOTER_Y, SCREEN_WIDTH, SCREEN_HEIGHT - FOOTER_Y, COLOR_BLUE);
    tft.setTextColor(ST77XX_BLACK);
    tft.setCursor(35, 113);
    tft.print("MO CONG RA...");
}

void showManualOpenScreen(const String& gateLabel, const String& reason) {
    invalidateIdleCache();
    currentScreenMode = SCREEN_MANUAL_OPEN;
    drawHeader(COLOR_ORANGE, "MANUAL OPEN", ST77XX_BLACK);
    drawSectionBanner("MO CONG THU CONG", COLOR_ORANGE, ST77XX_BLACK);

    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(1);

    tft.setCursor(10, 58);
    tft.print("Cong: ");
    tft.setTextColor(COLOR_BLUE);
    tft.print(gateLabel);

    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(10, 76);
    tft.print("Ly do: ");
    tft.setTextColor(COLOR_YELLOW);
    tft.print(reason);

    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(10, 94);
    tft.print("Trang thai: Dang mo");

    tft.fillRect(0, FOOTER_Y, SCREEN_WIDTH, SCREEN_HEIGHT - FOOTER_Y, COLOR_ORANGE);
    tft.setTextColor(ST77XX_BLACK);
    tft.setCursor(18, 113);
    tft.print("DA GUI LEN WEB...");
}

void showWarningScreen(const String& message) {
    invalidateIdleCache();
    currentScreenMode = SCREEN_WARNING;
    drawHeader(COLOR_RED, "LOI HE THONG");
    drawSectionBanner("CANH BAO", COLOR_RED);

    tft.drawRoundRect(6, 52, 148, 46, 6, COLOR_RED);
    drawMultilineText(message, 12, 62, COLOR_YELLOW);

    tft.setTextColor(COLOR_RED);
    tft.setCursor(18, 104);
    tft.print("GIAO DICH BI TU CHOI");

    tft.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_RED);
    tft.drawRect(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_RED);
}
