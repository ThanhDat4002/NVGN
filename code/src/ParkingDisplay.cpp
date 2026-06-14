#include "ParkingDisplay.h"


// -------------------- BANG MAU --------------------
#define COL_BG          ST77XX_WHITE   // Nen sang -> chu de doc duoi anh sang manh
#define COL_TEXT        ST77XX_BLACK
#define COL_TEXT_SOFT   0x52AA         // Xam trung tinh cho label phu
#define COL_LINE        0xC618         // Xam nhat cho duong ke

#define COL_RED         0xF800
#define COL_DARK_RED    0xA000
#define COL_GREEN       0x0560         // Xanh la dam vua, doc tot tren nen trang
#define COL_DARK_GREEN  0x02E0
#define COL_BLUE        0x019F         // Xanh duong tuoi
#define COL_DARK_BLUE   0x000F
#define COL_ORANGE      0xFC60         // Cam tuoi
#define COL_YELLOW      0xFEA0
#define COL_FOOTER_BG   0x2104         // Xam than than cho footer trang Idle

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

namespace {

// -------------------- HE TOA DO BO CUC --------------------
constexpr int16_t SW = 160;
constexpr int16_t SH = 128;

constexpr int16_t HEADER_H   = 24;                       // 0..23  : dai do "BAI XE PTIT"
constexpr int16_t BANNER_Y   = HEADER_H + 2;             // 26
constexpr int16_t BANNER_H   = 18;                       // 26..43 : dai trang thai
constexpr int16_t BODY_Y     = BANNER_Y + BANNER_H + 4;  // 48
constexpr int16_t FOOTER_H   = 18;
constexpr int16_t FOOTER_Y   = SH - FOOTER_H;            // 110..127

enum ScreenMode {
    SCREEN_NONE,
    SCREEN_BOOT,
    SCREEN_IDLE,
    SCREEN_CHECKIN,
    SCREEN_CHECKOUT,
    SCREEN_MANUAL_BARRIER,
    SCREEN_CASH_PAYMENT_PENDING,
    SCREEN_WARNING
};

ScreenMode currentScreenMode = SCREEN_NONE;

// -------------------- TIEN ICH XU LY CHUOI --------------------

void replaceMany(String& text, const char* const* patterns, size_t count, const char* replacement) {
    for (size_t i = 0; i < count; ++i) {
        text.replace(patterns[i], replacement);
    }
}

// Chuyen ky tu tieng Viet co dau ve khong dau, va loc ASCII de hop voi font mac dinh
String sanitizeDisplayText(const String& input) {
    String text = input;

    static const char* const lowerA[] = {u8"à", u8"á", u8"ả", u8"ã", u8"ạ", u8"ă", u8"ằ", u8"ắ", u8"ẳ", u8"ẵ", u8"ặ", u8"â", u8"ầ", u8"ấ", u8"ẩ", u8"ẫ", u8"ậ"};
    static const char* const upperA[] = {u8"À", u8"Á", u8"Ả", u8"Ã", u8"Ạ", u8"Ă", u8"Ằ", u8"Ắ", u8"Ẳ", u8"Ẵ", u8"Ặ", u8"Â", u8"Ầ", u8"Ấ", u8"Ẩ", u8"Ẫ", u8"Ậ"};
    static const char* const lowerE[] = {u8"è", u8"é", u8"ẻ", u8"ẽ", u8"ẹ", u8"ê", u8"ề", u8"ế", u8"ể", u8"ễ", u8"ệ"};
    static const char* const upperE[] = {u8"È", u8"É", u8"Ẻ", u8"Ẽ", u8"Ẹ", u8"Ê", u8"Ề", u8"Ế", u8"Ể", u8"Ễ", u8"Ệ"};
    static const char* const lowerI[] = {u8"ì", u8"í", u8"ỉ", u8"ĩ", u8"ị"};
    static const char* const upperI[] = {u8"Ì", u8"Í", u8"Ỉ", u8"Ĩ", u8"Ị"};
    static const char* const lowerO[] = {u8"ò", u8"ó", u8"ỏ", u8"õ", u8"ọ", u8"ô", u8"ồ", u8"ố", u8"ổ", u8"ỗ", u8"ộ", u8"ơ", u8"ờ", u8"ớ", u8"ở", u8"ỡ", u8"ợ"};
    static const char* const upperO[] = {u8"Ò", u8"Ó", u8"Ỏ", u8"Õ", u8"Ọ", u8"Ô", u8"Ồ", u8"Ố", u8"Ổ", u8"Ỗ", u8"Ộ", u8"Ơ", u8"Ờ", u8"Ớ", u8"Ở", u8"Ỡ", u8"Ợ"};
    static const char* const lowerU[] = {u8"ù", u8"ú", u8"ủ", u8"ũ", u8"ụ", u8"ư", u8"ừ", u8"ứ", u8"ử", u8"ữ", u8"ự"};
    static const char* const upperU[] = {u8"Ù", u8"Ú", u8"Ủ", u8"Ũ", u8"Ụ", u8"Ư", u8"Ừ", u8"Ứ", u8"Ử", u8"Ữ", u8"Ự"};
    static const char* const lowerY[] = {u8"ỳ", u8"ý", u8"ỷ", u8"ỹ", u8"ỵ"};
    static const char* const upperY[] = {u8"Ỳ", u8"Ý", u8"Ỷ", u8"Ỹ", u8"Ỵ"};
    static const char* const lowerD[] = {u8"đ"};
    static const char* const upperD[] = {u8"Đ"};

    replaceMany(text, lowerA, sizeof(lowerA) / sizeof(lowerA[0]), "a");
    replaceMany(text, upperA, sizeof(upperA) / sizeof(upperA[0]), "A");
    replaceMany(text, lowerE, sizeof(lowerE) / sizeof(lowerE[0]), "e");
    replaceMany(text, upperE, sizeof(upperE) / sizeof(upperE[0]), "E");
    replaceMany(text, lowerI, sizeof(lowerI) / sizeof(lowerI[0]), "i");
    replaceMany(text, upperI, sizeof(upperI) / sizeof(upperI[0]), "I");
    replaceMany(text, lowerO, sizeof(lowerO) / sizeof(lowerO[0]), "o");
    replaceMany(text, upperO, sizeof(upperO) / sizeof(upperO[0]), "O");
    replaceMany(text, lowerU, sizeof(lowerU) / sizeof(lowerU[0]), "u");
    replaceMany(text, upperU, sizeof(upperU) / sizeof(upperU[0]), "U");
    replaceMany(text, lowerY, sizeof(lowerY) / sizeof(lowerY[0]), "y");
    replaceMany(text, upperY, sizeof(upperY) / sizeof(upperY[0]), "Y");
    replaceMany(text, lowerD, sizeof(lowerD) / sizeof(lowerD[0]), "d");
    replaceMany(text, upperD, sizeof(upperD) / sizeof(upperD[0]), "D");

    String asciiOnly = "";
    asciiOnly.reserve(text.length());
    for (size_t i = 0; i < text.length(); ++i) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch >= 32 && ch <= 126) {
            asciiOnly += static_cast<char>(ch);
        }
    }
    return asciiOnly;
}

inline void invalidateIdleCache() { currentScreenMode = SCREEN_NONE; }

inline int16_t textW(const String& s, uint8_t size) {
    return static_cast<int16_t>(s.length()) * 6 * size;
}

// Cat bot ky tu cuoi neu chuoi qua dai so voi maxChars
String fitWidth(const String& s, uint8_t size, int16_t maxPixels) {
    int16_t maxChars = maxPixels / (6 * size);
    if (static_cast<int16_t>(s.length()) <= maxChars) return s;
    if (maxChars <= 1) return s.substring(0, maxChars);
    return s.substring(0, maxChars - 1) + ".";
}

// -------------------- HAM VE CHUOI --------------------

void drawCenteredText(const String& text, int16_t y, uint16_t color, uint8_t size = 1) {
    int16_t x = (SW - textW(text, size)) / 2;
    if (x < 0) x = 0;
    tft.setTextColor(color);
    tft.setTextSize(size);
    tft.setCursor(x, y);
    tft.print(text);
}

// Mo phong "bold" bang cach in 2 lan lech 1 pixel theo chieu ngang
void drawBoldCenteredText(const String& text, int16_t y, uint16_t color, uint8_t size = 1) {
    int16_t x = (SW - textW(text, size)) / 2;
    if (x < 0) x = 0;
    tft.setTextColor(color);
    tft.setTextSize(size);
    tft.setCursor(x, y);
    tft.print(text);
    tft.setCursor(x + 1, y);
    tft.print(text);
}

void drawBoldTextAt(const String& text, int16_t x, int16_t y, uint16_t color, uint8_t size = 1) {
    tft.setTextColor(color);
    tft.setTextSize(size);
    tft.setCursor(x, y);
    tft.print(text);
    tft.setCursor(x + 1, y);
    tft.print(text);
}

void drawMultilineText(const String& text, int16_t x, int16_t y, uint16_t color, uint8_t size = 1) {
    String safeText = sanitizeDisplayText(text);
    tft.setTextColor(color);
    tft.setTextSize(size);

    int16_t lineStart = 0;
    int16_t currentY = y;
    int16_t lineHeight = 8 * size + 2;

    while (lineStart <= static_cast<int16_t>(safeText.length())) {
        int16_t lineEnd = safeText.indexOf('\n', lineStart);
        if (lineEnd < 0) lineEnd = safeText.length();

        String line = safeText.substring(lineStart, lineEnd);
        int16_t lineX = (SW - textW(line, size)) / 2;
        if (lineX < x) lineX = x;
        tft.setCursor(lineX, currentY);
        tft.print(line);

        currentY += lineHeight;
        if (lineEnd == static_cast<int16_t>(safeText.length())) break;
        lineStart = lineEnd + 1;
    }
}

// -------------------- THANH PHAN UI CHUNG --------------------

// Tieu de chung "BAI XE PTIT" o dau moi man hinh: dai do tuoi, chu trang
// dam, kich thuoc lon (size 2 = 16 px cao) -> de doc tu xa.
void drawAppHeader() {
    tft.fillRect(0, 0, SW, HEADER_H - 2, COL_RED);
    tft.fillRect(0, HEADER_H - 2, SW, 2, COL_DARK_RED);
    drawBoldCenteredText("BAI XE PTIT", 4, ST77XX_WHITE, 2);
}

// Dai banner ngay duoi header, dung lam tieu de phu cho moi man hinh
void drawBanner(const String& text, uint16_t bg, uint16_t fg, uint8_t size = 1) {
    tft.fillRect(0, BANNER_Y, SW, BANNER_H, bg);
    int16_t y = BANNER_Y + (BANNER_H - 8 * size) / 2;
    drawBoldCenteredText(text, y, fg, size);
}

// Dai footer hanh dong (vd: "MO CONG VAO...")
void drawActionFooter(const String& text, uint16_t bg, uint16_t fg, uint8_t size = 1) {
    tft.fillRect(0, FOOTER_Y, SW, FOOTER_H, bg);
    int16_t y = FOOTER_Y + (FOOTER_H - 8 * size) / 2;
    drawBoldCenteredText(text, y, fg, size);
}

// =========================================================================
//  BITMAP ICON WIFI 32 x 32 (monochrome, MSB-first, 1 bit/pixel)
//  Mau cua nguoi dung: 3 cung tron + 1 cham, viet san trong 32x32 frame.
//  Adafruit GFX se chi to pixel "1", pixel "0" trong suot (giu nen).
// =========================================================================
static const unsigned char wifi_icon_32x32[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x7F, 0xFE, 0x00,
    0x03, 0xFF, 0xFF, 0xC0,
    0x0F, 0xC0, 0x03, 0xF0,
    0x1F, 0x00, 0x00, 0xF8,
    0x3C, 0x00, 0x00, 0x3C,
    0x70, 0x1F, 0xF8, 0x0E,
    0x60, 0xFF, 0xFF, 0x06,
    0x03, 0xF0, 0x0F, 0xC0,
    0x07, 0xC0, 0x03, 0xE0,
    0x0E, 0x00, 0x00, 0x70,
    0x0C, 0x0F, 0xF0, 0x30,
    0x00, 0x7F, 0xFE, 0x00,
    0x00, 0xF8, 0x1F, 0x00,
    0x01, 0xE0, 0x07, 0x80,
    0x01, 0xC0, 0x03, 0x80,
    0x00, 0x03, 0xC0, 0x00,
    0x00, 0x0F, 0xF0, 0x00,
    0x00, 0x0E, 0x70, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x80, 0x00,
    0x00, 0x03, 0xC0, 0x00,
    0x00, 0x07, 0xE0, 0x00,
    0x00, 0x07, 0xE0, 0x00,
    0x00, 0x03, 0xC0, 0x00,
    0x00, 0x01, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

// Ve bitmap WiFi 32x32 voi mau tuy chon, nen trong suot.
//   topLeftX, topLeftY : toa do goc trai-tren cua khung 32x32
//   color              : mau pixel "1" (1-bit bitmap)
void drawBigWifiIcon(int16_t topLeftX, int16_t topLeftY, uint16_t color) {
    tft.drawBitmap(topLeftX, topLeftY, wifi_icon_32x32, 32, 32, color);
}

// Ve mot truong du lieu kieu "the": label nho mau xam + value to mau noi bat.
// Chieu cao chuan ~22 px khi value dung size 2.
void drawFieldRow(const String& label,
                  const String& value,
                  int16_t y,
                  uint16_t valueColor,
                  uint8_t valueSize = 2) {
    // Label nho phia tren
    tft.setTextColor(COL_TEXT_SOFT);
    tft.setTextSize(1);
    tft.setCursor(8, y);
    tft.print(label);

    // Value to ngay duoi label, co kich thuoc neu qua dai
    int16_t valueY = y + 9;
    int16_t maxPixels = SW - 16;
    String shown = fitWidth(value, valueSize, maxPixels);
    int16_t valueX = 8;
    tft.setTextColor(valueColor);
    tft.setTextSize(valueSize);
    tft.setCursor(valueX, valueY);
    tft.print(shown);
}

// Ve 1 truong inline: "Label: value" cung tren mot dong size 1
void drawInlineField(const String& label,
                     const String& value,
                     int16_t x,
                     int16_t y,
                     uint16_t valueColor) {
    tft.setTextSize(1);
    tft.setTextColor(COL_TEXT_SOFT);
    tft.setCursor(x, y);
    tft.print(label);

    int16_t valueX = x + label.length() * 6;
    int16_t maxPixels = SW - valueX - 4;
    tft.setTextColor(valueColor);
    tft.setCursor(valueX, y);
    tft.print(fitWidth(value, 1, maxPixels));
}

}  // namespace (anonymous)

// =========================================================================
//                              API CONG KHAI
// =========================================================================

void initDisplay() {
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    tft.fillScreen(COL_BG);
    tft.setTextWrap(false);
}

// -------------------- MAN HINH KHOI DONG --------------------
void showBootingScreen(const String& step, int progressPercent) {
    invalidateIdleCache();
    currentScreenMode = SCREEN_BOOT;

    tft.fillScreen(COL_BG);
    drawAppHeader();
    drawBanner("KHOI DONG HE THONG", COL_BLUE, ST77XX_WHITE, 1);

    drawBoldCenteredText("BAI DO XE PTIT", 54, COL_DARK_BLUE, 2);
    drawCenteredText("System Initializing...", 76, COL_TEXT_SOFT, 1);

    const int barWidth  = 132;
    const int barHeight = 10;
    const int barX = (SW - barWidth) / 2;
    const int barY = 90;

    tft.drawRoundRect(barX, barY, barWidth, barHeight, 3, COL_TEXT);
    int fill = (barWidth - 4) * progressPercent / 100;
    if (fill < 0) fill = 0;
    if (fill > barWidth - 4) fill = barWidth - 4;
    tft.fillRoundRect(barX + 2, barY + 2, fill, barHeight - 4, 2, COL_BLUE);

    drawCenteredText(sanitizeDisplayText(step), 108, COL_TEXT, 1);
}

// -------------------- MAN HINH CHO (IDLE) --------------------
// Header: "BAI XE PTIT" (do)
// Banner: "SAN SANG QUET THE" (do)
// Body  : WiFi icon to (mau theo trang thai WiFi) + chu "MQTT" to (mau theo MQTT)
// Footer: ngay gio + chi bao W/M
void showIdleScreen(const String& dateTimeStr, int parkedCount, bool wifiOk, bool mqttOk) {
    (void)parkedCount;

    static bool lastWifiOk = false;
    static bool lastMqttOk = false;
    static String lastDisplayTime = "";

    String displayTime = dateTimeStr;
    if (dateTimeStr.length() >= 16) {
        displayTime = dateTimeStr.substring(2, 16);  // YY-MM-DD HH:MM
    }

    bool fullRedraw = (currentScreenMode != SCREEN_IDLE);

    if (fullRedraw) {
        tft.fillScreen(COL_BG);
        drawAppHeader();
        drawBanner("SAN SANG QUET THE", COL_RED, ST77XX_WHITE, 1);

        currentScreenMode = SCREEN_IDLE;
        lastWifiOk = !wifiOk;
        lastMqttOk = !mqttOk;
        lastDisplayTime = "";
    }

    bool connectionChanged = (wifiOk != lastWifiOk) || (mqttOk != lastMqttOk);
    bool timeChanged = (displayTime != lastDisplayTime);

    if (fullRedraw || connectionChanged) {
        // Xoa vung body
        tft.fillRect(0, BODY_Y, SW, FOOTER_Y - BODY_Y, COL_BG);

        // Bitmap icon WiFi 32x32 can giua ngang, dat tai (x=64, y=48)
        // Phan noi dung icon nam trong rows 1..25 cua bitmap -> chu MQTT
        // dat ngay duoi noi dung visible (y ~ 78) van trong khung bitmap.
        uint16_t wifiColor = wifiOk ? COL_GREEN : COL_RED;
        const int16_t iconX = (SW - 32) / 2;  // 64
        const int16_t iconY = 48;
        drawBigWifiIcon(iconX, iconY, wifiColor);

        // Chu "MQTT" to (size 2), mau theo trang thai MQTT, ngay duoi icon
        uint16_t mqttColor = mqttOk ? COL_GREEN : COL_RED;
        drawBoldCenteredText("MQTT", 86, mqttColor, 2);

        lastWifiOk = wifiOk;
        lastMqttOk = mqttOk;
        timeChanged = true;
    }

    if (timeChanged) {
        tft.fillRect(0, FOOTER_Y, SW, FOOTER_H, COL_FOOTER_BG);
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(1);
        tft.setCursor(6, FOOTER_Y + 5);
        tft.print(displayTime);

        // Chi bao W/M phia phai
        tft.setCursor(SW - 36, FOOTER_Y + 5);
        tft.setTextColor(wifiOk ? COL_GREEN : COL_RED);
        tft.print("W");
        tft.setTextColor(ST77XX_WHITE);
        tft.print("/");
        tft.setTextColor(mqttOk ? COL_GREEN : COL_RED);
        tft.print("M");

        lastDisplayTime = displayTime;
    }
}

// -------------------- MAN HINH XE VAO (CHECK-IN) --------------------
void showCheckInSuccess(const String& name, const String& plate, const String& entryTime) {
    String safeName  = sanitizeDisplayText(name);
    String safePlate = sanitizeDisplayText(plate);

    invalidateIdleCache();
    currentScreenMode = SCREEN_CHECKIN;

    tft.fillScreen(COL_BG);
    drawAppHeader();
    drawBanner("XE VAO BAI", COL_GREEN, ST77XX_WHITE, 2);

    // 3 dong du lieu - moi dong: label + value CUNG MOT HANG
    drawInlineField("Chu xe : ", safeName,  8, 54, COL_TEXT);
    drawInlineField("Bien so: ", safePlate, 8, 70, COL_DARK_BLUE);

    String timeOnly = entryTime;
    if (entryTime.length() > 11) {
        timeOnly = entryTime.substring(11);
    }
    drawInlineField("Gio vao: ", timeOnly, 8, 86, COL_TEXT);

    drawActionFooter("MO CONG VAO...", COL_GREEN, ST77XX_WHITE, 1);
}

// -------------------- MAN HINH XE RA (CHECK-OUT) --------------------
// Hien thi: ten, bien so, phi gui xe. KHONG hien thi so du.
void showCheckOutSuccess(const String& name, const String& plate, long fee) {
    String safeName  = sanitizeDisplayText(name);
    String safePlate = sanitizeDisplayText(plate);

    invalidateIdleCache();
    currentScreenMode = SCREEN_CHECKOUT;

    tft.fillScreen(COL_BG);
    drawAppHeader();
    drawBanner("XE RA BAI", COL_BLUE, ST77XX_WHITE, 2);

    // 3 dong du lieu - moi dong: label + value CUNG MOT HANG
    drawInlineField("Chu xe : ", safeName,  8, 54, COL_TEXT);
    drawInlineField("Bien so: ", safePlate, 8, 70, COL_DARK_BLUE);

    // Hang 3: "PHI GUI XE: 5000 d" - label va value LIEN NHAU tren cung 1 hang
    //   - Label size 1, vertical-center voi value size 2 (lech 4 px xuong)
    //   - Value size 2 mau do, dat ngay sau label
    const String feeLabel = "PHI GUI XE:";
    String feeStr = " " + String(fee) + " d";

    int16_t labelW = textW(feeLabel, 1);
    int16_t labelX = 8;
    int16_t valueX = labelX + labelW + 2;

    tft.setTextSize(1);
    tft.setTextColor(COL_TEXT_SOFT);
    tft.setCursor(labelX, 88);
    tft.print(feeLabel);

    drawBoldTextAt(feeStr, valueX, 84, COL_RED, 2);

    drawActionFooter("MO CONG RA...", COL_BLUE, ST77XX_WHITE, 1);
}

// -------------------- MAN HINH DIEU KHIEN CONG THU CONG --------------------
void showManualBarrierScreen(const String& gateLabel, bool isOpening, const String& reason) {
    String safeGate   = sanitizeDisplayText(gateLabel);
    String safeReason = sanitizeDisplayText(reason);

    invalidateIdleCache();
    currentScreenMode = SCREEN_MANUAL_BARRIER;

    tft.fillScreen(COL_BG);
    drawAppHeader();
    drawBanner("DIEU KHIEN THU CONG", COL_ORANGE, ST77XX_BLACK, 1);

    // Hanh dong lon o giua
    const char* action = isOpening ? "DANG MO" : "DANG DONG";
    drawBoldCenteredText(action, 52, isOpening ? COL_GREEN : COL_RED, 2);

    // Thong tin chi tiet
    drawInlineField("CONG : ", safeGate,   8, 78, COL_DARK_BLUE);
    drawInlineField("LY DO: ", safeReason, 8, 92, COL_TEXT);

    drawActionFooter(isOpening ? "DA MO & DONG BO" : "DA DONG & DONG BO",
                     COL_ORANGE, ST77XX_BLACK, 1);
}

// -------------------- MAN HINH CHO THANH TOAN TIEN MAT --------------------
// Khi the khong du so du -> hien thi ten, bien so, so tien can thu va
// nhac nguoi van hanh bam nut CONG RA trong 30s de xac nhan da thu tien mat.
void showCashPaymentPendingScreen(const String& name, const String& plate, long fee) {
    String safeName  = sanitizeDisplayText(name);
    String safePlate = sanitizeDisplayText(plate);

    invalidateIdleCache();
    currentScreenMode = SCREEN_CASH_PAYMENT_PENDING;

    tft.fillScreen(COL_BG);
    drawAppHeader();
    drawBanner("THU TIEN MAT", COL_ORANGE, ST77XX_BLACK, 2);

    // 3 dong du lieu - moi dong: label + value CUNG MOT HANG
    drawInlineField("Chu xe : ", safeName,  8, 50, COL_TEXT);
    drawInlineField("Bien so: ", safePlate, 8, 64, COL_DARK_BLUE);

    // Hang 3: "CAN THU: 5000 d" - label va value LIEN NHAU tren cung 1 hang
    const String feeLabel = "CAN THU:";
    String feeStr = " " + String(fee) + " d";

    int16_t labelW = textW(feeLabel, 1);
    int16_t labelX = 8;
    int16_t valueX = labelX + labelW + 2;

    tft.setTextSize(1);
    tft.setTextColor(COL_TEXT_SOFT);
    tft.setCursor(labelX, 82);
    tft.print(feeLabel);

    drawBoldTextAt(feeStr, valueX, 78, COL_RED, 2);

    // Huong dan thao tac
    drawCenteredText("Bam CONG RA trong 30s", 102, COL_DARK_BLUE, 1);

    drawActionFooter("CHO XAC NHAN TIEN MAT", COL_ORANGE, ST77XX_BLACK, 1);
}

// -------------------- MAN HINH CANH BAO / LOI --------------------
void showWarningScreen(const String& message) {
    invalidateIdleCache();
    currentScreenMode = SCREEN_WARNING;

    tft.fillScreen(COL_BG);
    drawAppHeader();
    drawBanner("CANH BAO", COL_RED, ST77XX_WHITE, 2);

    // Vung "the" canh bao: vien do dam, chu vang san khac noi bat
    tft.drawRoundRect(6, 50, SW - 12, 50, 6, COL_RED);
    tft.drawRoundRect(7, 51, SW - 14, 48, 5, COL_RED);

    String safeMsg = sanitizeDisplayText(message);
    // Neu thong diep ngan, ve size 2; neu dai, ve size 1
    if (textW(safeMsg, 2) <= SW - 24) {
        drawBoldCenteredText(safeMsg, 65, COL_DARK_RED, 2);
    } else {
        drawMultilineText(safeMsg, 12, 60, COL_DARK_RED, 1);
    }
    drawCenteredText("- TU CHOI GIAO DICH -", 86, COL_TEXT_SOFT, 1);

    drawActionFooter("VUI LONG THU LAI", COL_RED, ST77XX_WHITE, 1);
}
