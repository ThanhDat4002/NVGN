#ifndef BARRIER_CONTROLLER_H
#define BARRIER_CONTROLLER_H

#include <Arduino.h>

// =========================================================================
//  BarrierController
//  ------------------------------------------------------------
//  Quan ly hai servo barrier (cong vao / cong ra):
//      - Khoi tao timer PWM va servo
//      - Mo / Dong tung cong, ghi nhan trang thai va thoi diem tu dong dong
//      - Goi maintainBarriers() moi vong loop() de tu dong dong sau delay
// =========================================================================

// Khoi tao servo, phai goi sau khi ESP32PWM duoc cap phat timer.
void initBarriers();

// Mo cong vao / ra: dong thoi phat bip, ghi log, gui status MQTT
void openBarrierIn(const char* source);
void openBarrierOut(const char* source);

// Dong cong vao / ra
void closeBarrierIn(const char* source);
void closeBarrierOut(const char* source);

// Tra ve "open" hoac "closed" - dung cho heartbeat MQTT
const char* getBarrierInState();
const char* getBarrierOutState();

// Co cong dang mo khong? (de phuc vu logic khac)
bool isBarrierInOpen();
bool isBarrierOutOpen();

// Tu dong dong cong sau khi het thoi gian delay. Goi moi vong loop().
void maintainBarriers(uint32_t currentMillis);

#endif // BARRIER_CONTROLLER_H
