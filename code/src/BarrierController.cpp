#include "BarrierController.h"

#include <ESP32Servo.h>

#include "Config.h"
#include "Buzzer.h"
#include "WifiMqttClient.h"     // sendDeviceStatus() khi co thay doi trang thai

namespace {
Servo servoIn;
Servo servoOut;

const char* barrierInState  = "closed";
const char* barrierOutState = "closed";

uint32_t barrierInCloseTime  = 0;
uint32_t barrierOutCloseTime = 0;
}  // namespace

void initBarriers() {
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);

    servoIn.setPeriodHertz(50);
    servoOut.setPeriodHertz(50);

    servoIn.attach(SERVO_IN_PIN, 500, 2400);
    servoOut.attach(SERVO_OUT_PIN, 500, 2400);

    // Dat barrier o trang thai dong mac dinh
    servoIn.write(BARRIER_CLOSED_ANGLE);
    servoOut.write(BARRIER_CLOSED_ANGLE);
}

void openBarrierIn(const char* source) {
    servoIn.write(BARRIER_OPEN_ANGLE);
    barrierInState = "open";
    barrierInCloseTime = millis() + BARRIER_DELAY_MS;
    Serial.printf("[+] Mo barrier Cong Vao bang %s.\n", source);
    beepSuccess();
    sendDeviceStatus();
}

void closeBarrierIn(const char* source) {
    servoIn.write(BARRIER_CLOSED_ANGLE);
    barrierInState = "closed";
    barrierInCloseTime = 0;
    Serial.printf("[+] Dong barrier CONG VAO bang %s.\n", source);
    sendDeviceStatus();
}

void openBarrierOut(const char* source) {
    servoOut.write(BARRIER_OPEN_ANGLE);
    barrierOutState = "open";
    barrierOutCloseTime = millis() + BARRIER_DELAY_MS;
    Serial.printf("[+] Mo barrier Cong Ra bang %s.\n", source);
    beepSuccess();
    sendDeviceStatus();
}

void closeBarrierOut(const char* source) {
    servoOut.write(BARRIER_CLOSED_ANGLE);
    barrierOutState = "closed";
    barrierOutCloseTime = 0;
    Serial.printf("[+] Dong barrier CONG RA bang %s.\n", source);
    sendDeviceStatus();
}

const char* getBarrierInState()  { return barrierInState; }
const char* getBarrierOutState() { return barrierOutState; }
bool isBarrierInOpen()  { return strcmp(barrierInState, "open") == 0; }
bool isBarrierOutOpen() { return strcmp(barrierOutState, "open") == 0; }

void maintainBarriers(uint32_t currentMillis) {
    if (isBarrierInOpen() && currentMillis > barrierInCloseTime) {
        closeBarrierIn("tu dong");
    }
    if (isBarrierOutOpen() && currentMillis > barrierOutCloseTime) {
        closeBarrierOut("tu dong");
    }
}
