#ifndef RFID_GATES_H
#define RFID_GATES_H

#include <Arduino.h>

// =========================================================================
//  RfidGates
//  ------------------------------------------------------------
//  Quan ly 2 dau doc MFRC522 (cong vao / cong ra) dung chung SPI bus.
//      - Khoi tao, kiem tra version
//      - Quet tuan tu trong loop(), debounce UID
//      - Khi co the moi: goi callback do main.cpp dang ky truoc
//
//  Cach dung:
//      initRfidGates(processCheckIn, processCheckOut);
//      ...
//      void loop() {
//          pollRfidGates();
//      }
// =========================================================================

typedef void (*RfidScanCallback)(const String& uid);

// Khoi tao hai dau doc va luu callback cho moi cong.
// Tra ve true neu CA HAI dau doc deu OK.
bool initRfidGates(RfidScanCallback onScanInGate, RfidScanCallback onScanOutGate);

// Quet ca 2 dau doc, dispatch callback khi co UID hop le.
// Goi moi vong loop().
void pollRfidGates();

// Trang thai phan cung tung dau doc (phuc vu heartbeat MQTT)
bool isRfidInReady();
bool isRfidOutReady();

// Reset SPI bus khi can (vd: sau xung dot SPI voi TFT).
void resetRfidBus();

#endif // RFID_GATES_H
