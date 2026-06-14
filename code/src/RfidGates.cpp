#include "RfidGates.h"

#include <MFRC522.h>

#include "Config.h"

namespace {
MFRC522 nfcIn(RC522_IN_SS, RC522_RST_PIN);
MFRC522 nfcOut(RC522_OUT_SS, RC522_RST_PIN);

bool isRfidInOk  = false;
bool isRfidOutOk = false;

RfidScanCallback onInGateScan  = nullptr;
RfidScanCallback onOutGateScan = nullptr;

// Debounce - chong rung khi the van dat tren dau doc
String   lastScannedUid = "";
uint32_t lastScanTime   = 0;
constexpr uint32_t DEBOUNCE_TIME_MS = 5000;

bool initReader(MFRC522& reader, const char* gateLabel) {
    resetRfidBus();
    reader.PCD_Init();
    delay(50);

    byte version = reader.PCD_ReadRegister(MFRC522::VersionReg);
    if (version == 0x00 || version == 0xFF) {
        Serial.printf("[-] Khong tim thay RC522 %s! VersionReg=0x%02X\n", gateLabel, version);
        return false;
    }
    Serial.printf("[+] Da tim thay RC522 %s. VersionReg=0x%02X\n", gateLabel, version);
    reader.PCD_AntennaOn();
    return true;
}

// Doc UID hien tai cua dau doc thanh chuoi HEX viet hoa.
String readUidHex(MFRC522& reader) {
    String uidStr = "";
    uint8_t len = reader.uid.size;
    for (uint8_t i = 0; i < len; i++) {
        byte v = reader.uid.uidByte[i];
        if (v < 0x10) uidStr += "0";
        uidStr += String(v, HEX);
    }
    uidStr.toUpperCase();
    return uidStr;
}

// Xu ly mot lan quet the tai 1 dau doc: doc UID -> debounce -> goi callback.
// Tra ve true neu da bat duoc the (du co callback hay khong).
bool scanReader(MFRC522& reader, const char* gateName, RfidScanCallback callback) {
    if (!reader.PICC_IsNewCardPresent() || !reader.PICC_ReadCardSerial()) {
        return false;
    }

    String uidStr = readUidHex(reader);
    uint32_t now = millis();

    if (uidStr != lastScannedUid || (now - lastScanTime > DEBOUNCE_TIME_MS)) {
        lastScannedUid = uidStr;
        lastScanTime = now;
        Serial.printf("\n[Card] Cong %s phat hien the UID: %s\n", gateName, uidStr.c_str());
        if (callback) callback(uidStr);
    }

    reader.PICC_HaltA();
    reader.PCD_StopCrypto1();
    return true;
}
}  // namespace

// =========================================================================
//  API PUBLIC
// =========================================================================

bool initRfidGates(RfidScanCallback onScanInGate, RfidScanCallback onScanOutGate) {
    onInGateScan  = onScanInGate;
    onOutGateScan = onScanOutGate;

    isRfidInOk  = initReader(nfcIn,  "Cong Vao");
    isRfidOutOk = initReader(nfcOut, "Cong Ra");
    return isRfidInOk && isRfidOutOk;
}

void pollRfidGates() {
    // Quet IN truoc, neu co the thi return de tranh xung dot SPI ngay lap tuc.
    if (isRfidInOk && scanReader(nfcIn, "VAO", onInGateScan)) {
        return;
    }
    if (isRfidOutOk) {
        scanReader(nfcOut, "RA", onOutGateScan);
    }
}

bool isRfidInReady()  { return isRfidInOk; }
bool isRfidOutReady() { return isRfidOutOk; }

void resetRfidBus() {
    digitalWrite(RC522_IN_SS, HIGH);
    digitalWrite(RC522_OUT_SS, HIGH);
    digitalWrite(RC522_RST_PIN, LOW);
    delay(20);
    digitalWrite(RC522_RST_PIN, HIGH);
    delay(50);
}
