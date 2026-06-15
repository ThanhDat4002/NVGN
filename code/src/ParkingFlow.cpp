#include "ParkingFlow.h"

#include "Config.h"
#include "Database.h"
#include "Buzzer.h"
#include "ParkingDisplay.h"
#include "TimeManager.h"
#include "BarrierController.h"
#include "WifiMqttClient.h"

// =========================================================================
//  TRANG THAI NOI BO
// =========================================================================
namespace {
// --- Cash checkout state ---
bool     cashCheckoutPending    = false;
String   cashCheckoutUid        = "";
long     cashCheckoutFee        = 0;
uint32_t cashCheckoutExpireTime = 0;

// --- UI busy state ---
uint32_t displayResetTime      = 0;
bool     isDisplayActiveMessage = false;

// --- Manual OUT button guard ---
uint32_t manualOutIgnoreUntil = 0;

// =========================================================================
//  TINH PHI GUI XE
// =========================================================================
long calculateParkingFee(const String& entryTimeStr, const DateTime& exitTime) {
    int ey, em, ed, eh, emin;
    if (!parseDateTime(entryTimeStr, ey, em, ed, eh, emin)) {
        // Neu phan tich thoi gian loi, ap dung phi co ban (Daytime) lam mac dinh
        return FEE_DAYTIME;
    }

    DateTime entryDate(ey, em, ed, 0, 0, 0);
    DateTime exitDate(exitTime.year(), exitTime.month(), exitTime.day(), 0, 0, 0);
    long overnightCount = 0;
    if (exitDate.unixtime() > entryDate.unixtime()) {
        overnightCount = static_cast<long>((exitDate.unixtime() - entryDate.unixtime()) / 86400UL);
    }

    if (overnightCount > 0) {
        long fee = overnightCount * FEE_OVERNIGHT;
        if (exitTime.hour() < FEE_NEXT_DAY_FREE_UNTIL_HOUR) {
            return fee;
        }
        if (exitTime.hour() >= 18) {
            return fee + FEE_NIGHTTIME;
        }
        return fee + FEE_DAYTIME;
    }

    // Cung ngay - xe ra sau 18:00
    if (exitTime.hour() >= 18) {
        return FEE_NIGHTTIME;
    }
    // Xe vao va ra cung ngay truoc 18:00
    return FEE_DAYTIME;
}

// =========================================================================
//  KHOI TAO PHIEN CHO THANH TOAN TIEN MAT
// =========================================================================
void startCashCheckout(const User& user, long fee) {
    cashCheckoutPending    = true;
    cashCheckoutUid        = user.uid;
    cashCheckoutFee        = fee;
    cashCheckoutExpireTime = millis() + CASH_CONFIRM_MS;
    manualOutIgnoreUntil   = 0;  // Cho phep bam nut OUT ngay de xac nhan tien mat

    Serial.printf("[WAIT_CASH_STARTED] UID=%s FEE=%ld BALANCE=%ld TIMEOUT_MS=%lu\n",
                  user.uid.c_str(), fee, user.balance,
                  static_cast<unsigned long>(CASH_CONFIRM_MS));

    isDisplayActiveMessage = true;
    displayResetTime       = cashCheckoutExpireTime;
    showCashPaymentPendingScreen(user.name, user.plateNumber, fee);
    publishParkingEvent(user.uid, "checkout_wait_cash",
                        getFormattedTime(getCurrentDateTime()),
                        fee, user.balance);
}
}  // namespace

// =========================================================================
//  CASH CHECKOUT PUBLIC API
// =========================================================================
bool isCashCheckoutPending() {
    return cashCheckoutPending && millis() <= cashCheckoutExpireTime;
}

void confirmCashCheckout() {
    if (!cashCheckoutPending) return;

    User user;
    if (!getUser(cashCheckoutUid, user)) {
        clearCashCheckout();
        return;
    }

    user.inParking = false;
    user.entryTime = "";
    saveUser(user);

    String confirmTime = getFormattedTime(getCurrentDateTime());
    isDisplayActiveMessage = true;
    displayResetTime       = millis() + 4000;
    manualOutIgnoreUntil   = millis() + MANUAL_OUT_BUTTON_GUARD_MS;

    Serial.printf("[WAIT_CASH_CONFIRMED] UID=%s FEE=%ld TIME=%s\n",
                  user.uid.c_str(), cashCheckoutFee, confirmTime.c_str());
    showCheckOutSuccess(user.name, user.plateNumber, cashCheckoutFee);
    openBarrierOut("xac nhan tien mat");
    publishParkingEvent(user.uid, "checkout", confirmTime, cashCheckoutFee, user.balance);
    clearCashCheckout();
}

void clearCashCheckout() {
    cashCheckoutPending    = false;
    cashCheckoutUid        = "";
    cashCheckoutFee        = 0;
    cashCheckoutExpireTime = 0;
}

void maintainCashCheckout(uint32_t currentMillis) {
    if (cashCheckoutPending && currentMillis > cashCheckoutExpireTime) {
        Serial.println("[WAIT_CASH_TIMEOUT] Het thoi gian xac nhan thanh toan tien mat.");
        clearCashCheckout();
        isDisplayActiveMessage = false;
    }
}

// =========================================================================
//  DISPLAY BUSY STATE
// =========================================================================
void markDisplayBusyFor(uint32_t durationMs) {
    isDisplayActiveMessage = true;
    displayResetTime = millis() + durationMs;
}

bool isDisplayBusy(uint32_t currentMillis) {
    if (!isDisplayActiveMessage) return false;
    if (currentMillis > displayResetTime) {
        isDisplayActiveMessage = false;
        return false;
    }
    return true;
}

void clearDisplayBusy() {
    isDisplayActiveMessage = false;
}

// =========================================================================
//  MANUAL OUT BUTTON GUARD
// =========================================================================
void setManualOutGuard(uint32_t untilMs) {
    manualOutIgnoreUntil = untilMs;
}

bool isWithinManualOutGuard(uint32_t currentMillis) {
    return currentMillis < manualOutIgnoreUntil;
}

// =========================================================================
//  CHECK-IN
// =========================================================================
void processCheckIn(const String& uid) {
    User user;
    markDisplayBusyFor(4000);

    // 1. Kiem tra the trong CSDL cuc bo
    if (!getUser(uid, user)) {
        showWarningScreen("THE CHUA DANG KY!");
        beepError();
        publishScan(uid);
        return;
    }

    // 2. Kiem tra the bi khoa khong
    if (user.status == "locked") {
        showWarningScreen("THE NAY DA BI KHOA!");
        beepError();
        return;
    }

    // 3. Kiem tra xe co dang nam trong bai khong
    if (user.inParking) {
        showWarningScreen("XE DANG TRONG BAI!");
        beepError();
        return;
    }

    // 4. Lay thoi gian tu RTC de cap nhat trang thai vao bai
    DateTime nowDt = getCurrentDateTime();
    String   timeStr = getFormattedTime(nowDt);
    Serial.printf("[CHECKIN] UID=%s ENTRY_TIME=%s BALANCE_BEFORE=%ld\n",
                  uid.c_str(), timeStr.c_str(), user.balance);

    user.inParking = true;
    user.entryTime = timeStr;
    saveUser(user);

    // 5. Hien thi thanh cong va mo barrier
    showCheckInSuccess(user.name, user.plateNumber, timeStr);
    openBarrierIn("quet the hop le");

    // 6. Dong bo su kien len MQTT
    publishParkingEvent(uid, "checkin", timeStr);
}

// =========================================================================
//  CHECK-OUT
// =========================================================================
void processCheckOut(const String& uid) {
    User user;
    markDisplayBusyFor(4000);

    // 1. Kiem tra the trong CSDL cuc bo
    if (!getUser(uid, user)) {
        showWarningScreen("THE CHUA DANG KY!");
        beepError();
        publishScan(uid);
        return;
    }

    // 2. Kiem tra the bi khoa
    if (user.status == "locked") {
        showWarningScreen("THE NAY DA BI KHOA!");
        beepError();
        return;
    }

    // 3. Kiem tra xe co o trong bai khong
    if (!user.inParking) {
        showWarningScreen("XE CHUA VAO BAI!");
        beepError();
        return;
    }

    // 4. Lay thoi gian va tinh phi
    DateTime nowDt       = getCurrentDateTime();
    String   exitTimeStr = getFormattedTime(nowDt);
    long     fee         = calculateParkingFee(user.entryTime, nowDt);

    Serial.printf("[CHECKOUT] UID=%s ENTRY_TIME=%s EXIT_TIME=%s LOCAL_FEE=%ld BALANCE_BEFORE=%ld\n",
                  uid.c_str(), user.entryTime.c_str(), exitTimeStr.c_str(), fee, user.balance);

    // 5. So du khong du -> chuyen sang luong cho thanh toan tien mat
    if (user.balance < fee) {
        startCashCheckout(user, fee);
        return;
    }

    // 6. Tru tien va cap nhat trang thai ra bai
    user.balance  -= fee;
    user.inParking = false;
    user.entryTime = "";
    saveUser(user);
    Serial.printf("[CHECKOUT] UID=%s BALANCE_AFTER_LOCAL=%ld\n", uid.c_str(), user.balance);

    // 7. Hien thi thong tin va mo barrier
    showCheckOutSuccess(user.name, user.plateNumber, fee);
    openBarrierOut("quet the hop le");

    // 8. Dong bo su kien
    publishParkingEvent(uid, "checkout", exitTimeStr, fee, user.balance);
}
