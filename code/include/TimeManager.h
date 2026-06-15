#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>

//  TimeManager
//  ------------------------------------------------------------
//  Quan ly toan bo thoi gian he thong:
//      - Khoi tao DS3231 qua I2C
//      - Lay/Format/Parse DateTime
//      - Dong bo NTP qua WiFi (mui gio Viet Nam GMT+7)
//      - Resync dinh ky de bu sai so DS3231
//
//  Doi tuong global noi bo (rtc, isRtcOk, fallback base, lastNtpSyncTime)
//  duoc dau goi trong file .cpp -> main.cpp khong can biet.

// Khoi tao DS3231 + Wire bus. Goi mot lan tu setup().
// - Neu tim thay RTC va RTC mat nguon: dat lai gio compile lam moc.
// - Neu khong tim thay RTC: chuyen sang che do fallback dua tren millis().
// Tra ve true neu phat hien duoc RTC vat ly, false neu fallback.
bool initTimeManager();

// Tra ve thoi gian hien tai (DS3231 neu co, hoac fallback theo millis()).
DateTime getCurrentDateTime();

// Format DateTime thanh chuoi "YYYY-MM-DD HH:MM:SS".
String getFormattedTime(const DateTime& dt);

// Parse chuoi "YYYY-MM-DD HH:MM" hoac "YYYY-MM-DD HH:MM:SS" thanh thanh phan.
// Tra ve true neu chuoi co dinh dang hop le.
bool parseDateTime(const String& str, int& y, int& m, int& d, int& h, int& min);

// Yeu cau dong bo gio tu NTP (pool.ntp.org / time.google.com / time.nist.gov).
// Mui gio cu the: GMT+7, khong DST.
//   - Doi WiFi ket noi truoc khi goi.
//   - Sau khi co gio chuan, ghi vao DS3231 bang rtc.adjust().
//   - Cap nhat dau thoi gian lan dong bo gan nhat de phuc vu maintainNtpSync().
// Tra ve true neu dong bo thanh cong trong timeoutMs.
bool syncRtcFromNtp(uint32_t timeoutMs = 8000);

// Goi tu loop() de tu dong dong bo NTP moi 1 gio (chong drift DS3231).
// Da kiem tra trang thai WiFi va dau thoi gian noi bo -> an toan goi moi vong.
// Tra ve true neu vua thuc hien mot lan sync.
bool maintainNtpSync(uint32_t currentMillis);

// Phan cung RTC co san sang khong?
bool isRtcAvailable();

#endif // TIME_MANAGER_H
