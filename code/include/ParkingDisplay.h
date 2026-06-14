#ifndef PARKING_DISPLAY_H
#define PARKING_DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "Config.h"

// Khởi tạo màn hình TFT ST7735
void initDisplay();

// Hiển thị màn hình khởi động (Booting)
void showBootingScreen(const String& step, int progressPercent);

// Hiển thị màn hình chính (Chờ)
void showIdleScreen(const String& dateTimeStr, int parkedCount, bool wifiOk, bool mqttOk);

// Hiển thị màn hình xe vào thành công (Check-In)
void showCheckInSuccess(const String& name, const String& plate, const String& entryTime);

// Hiển thị màn hình xe ra thành công (Check-Out)
void showCheckOutSuccess(const String& name, const String& plate, long fee);

// Hiển thị màn hình thao tác cổng thủ công
void showManualBarrierScreen(const String& gateLabel, bool isOpening, const String& reason);

// Hiển thị màn hình chờ xác nhận thanh toán tiền mặt
void showCashPaymentPendingScreen(const String& name, const String& plate, long fee);

// Hiển thị màn hình báo lỗi (Thất bại)
void showWarningScreen(const String& message);

#endif // PARKING_DISPLAY_H
