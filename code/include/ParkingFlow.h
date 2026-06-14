#ifndef PARKING_FLOW_H
#define PARKING_FLOW_H

#include <Arduino.h>

// =========================================================================
//  ParkingFlow
//  ------------------------------------------------------------
//  Toan bo nghiep vu cua bai xe:
//      - Xu ly the quet vao bai (check-in)
//      - Xu ly the quet ra bai (check-out)
//      - Tinh phi gui xe
//      - Quan ly trang thai cho thanh toan tien mat
//      - Quan ly co "man hinh dang ban" va guard cua nut OUT
//
//  Goi tu main.cpp / RfidGates / handleManualButtons.
// =========================================================================

// ----------------- Cac luong nghiep vu chinh -----------------
void processCheckIn(const String& uid);
void processCheckOut(const String& uid);

// ----------------- Thanh toan tien mat -----------------
// True khi dang cho nguoi van hanh xac nhan da thu tien mat.
bool isCashCheckoutPending();
// Goi khi nguoi van hanh bam nut OUT trong cua so cho xac nhan.
void confirmCashCheckout();
// Huy mode cho tien mat (timeout / nut sai / ...)
void clearCashCheckout();
// Kiem tra timeout cua phien cho tien mat. Goi moi vong loop().
void maintainCashCheckout(uint32_t currentMillis);

// ----------------- Co "man hinh dang ban" -----------------
// Idle screen chi duoc ghi de khi !isDisplayBusy().
void markDisplayBusyFor(uint32_t durationMs);
bool isDisplayBusy(uint32_t currentMillis);
void clearDisplayBusy();

// ----------------- Guard cho nut OUT -----------------
// Sau khi xu ly mot lan bam, dat khung thoi gian bo qua de tranh rung nut
// va tranh xu ly trung lap (vd: nguoi van hanh giu nut hoi lau).
void setManualOutGuard(uint32_t untilMs);
bool isWithinManualOutGuard(uint32_t currentMillis);

#endif // PARKING_FLOW_H
