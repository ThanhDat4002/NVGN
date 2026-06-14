#include "TimeManager.h"

#include <Wire.h>
#include <WiFi.h>
#include <time.h>

#include "Config.h"

// =========================================================================
//  TRANG THAI NOI BO (file-scope, an khoi cac module khac)
// =========================================================================
namespace {
RTC_DS3231 rtc;
bool        isRtcOk = false;
DateTime    rtcFallbackBase;
uint32_t    rtcFallbackStartMillis = 0;

uint32_t    lastNtpSyncTime = 0;
constexpr uint32_t NTP_RESYNC_INTERVAL_MS = 3600000UL; // 1 gio
}  // namespace

// =========================================================================
//  KHOI TAO RTC
// =========================================================================
bool initTimeManager() {
    Wire.begin(I2C_SDA, I2C_SCL);
    rtcFallbackBase = DateTime(F(__DATE__), F(__TIME__));
    rtcFallbackStartMillis = millis();

    isRtcOk = rtc.begin();
    if (!isRtcOk) {
        Serial.println("[-] Khong tim thay RTC DS3231!");
        Serial.println("[!] Su dung moc thoi gian fallback tu thoi diem nap firmware.");
        return false;
    }

    if (rtc.lostPower()) {
        Serial.println("[!] RTC mat nguon! Dang thiet lap lai gio compile.");
        rtc.adjust(rtcFallbackBase);
    }
    Serial.println("[+] RTC DS3231 khoi dong ok.");
    return true;
}

bool isRtcAvailable() {
    return isRtcOk;
}

// =========================================================================
//  LAY / FORMAT / PARSE DATETIME
// =========================================================================
DateTime getCurrentDateTime() {
    if (isRtcOk) {
        return rtc.now();
    }
    uint32_t elapsedSeconds = (millis() - rtcFallbackStartMillis) / 1000;
    return rtcFallbackBase + TimeSpan(elapsedSeconds);
}

String getFormattedTime(const DateTime& dt) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             dt.year(), dt.month(), dt.day(),
             dt.hour(), dt.minute(), dt.second());
    return String(buf);
}

bool parseDateTime(const String& str, int& y, int& m, int& d, int& h, int& min) {
    if (str.length() < 16) return false;
    y   = str.substring(0, 4).toInt();
    m   = str.substring(5, 7).toInt();
    d   = str.substring(8, 10).toInt();
    h   = str.substring(11, 13).toInt();
    min = str.substring(14, 16).toInt();
    return true;
}

// =========================================================================
//  DONG BO NTP -> DS3231
//  Mui gio Viet Nam: GMT+7, khong DST -> offset = 7 * 3600s
// =========================================================================
bool syncRtcFromNtp(uint32_t timeoutMs) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[NTP] Khong co WiFi -> bo qua dong bo NTP.");
        return false;
    }

    Serial.println("[NTP] Yeu cau gio tu pool.ntp.org / time.google.com ...");
    configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");

    struct tm timeinfo;
    uint32_t startMs = millis();
    bool ok = false;
    while ((millis() - startMs) < timeoutMs) {
        if (getLocalTime(&timeinfo, 200) && timeinfo.tm_year > (2020 - 1900)) {
            ok = true;
            break;
        }
        delay(100);
    }

    if (!ok) {
        Serial.println("[NTP] Khong lay duoc gio (timeout).");
        return false;
    }

    DateTime ntpTime(timeinfo.tm_year + 1900,
                     timeinfo.tm_mon + 1,
                     timeinfo.tm_mday,
                     timeinfo.tm_hour,
                     timeinfo.tm_min,
                     timeinfo.tm_sec);

    if (isRtcOk) {
        rtc.adjust(ntpTime);
        Serial.printf("[NTP] DA DONG BO DS3231 -> %04d-%02d-%02d %02d:%02d:%02d (GMT+7)\n",
                      ntpTime.year(), ntpTime.month(), ntpTime.day(),
                      ntpTime.hour(), ntpTime.minute(), ntpTime.second());
    } else {
        rtcFallbackBase = ntpTime;
        rtcFallbackStartMillis = millis();
        Serial.println("[NTP] Da cap nhat fallback time (khong co RTC vat ly).");
    }

    lastNtpSyncTime = millis();
    if (lastNtpSyncTime == 0) lastNtpSyncTime = 1; // tranh tra ve 0
    return true;
}

bool maintainNtpSync(uint32_t currentMillis) {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (lastNtpSyncTime != 0 &&
        (currentMillis - lastNtpSyncTime) <= NTP_RESYNC_INTERVAL_MS) {
        return false;
    }
    return syncRtcFromNtp(5000);
}
