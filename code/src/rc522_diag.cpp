#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#include "Config.h"

MFRC522 rfidIn(RC522_IN_SS, RC522_RST_PIN);
MFRC522 rfidOut(RC522_OUT_SS, RC522_RST_PIN);

bool isRfidInOk = false;
bool isRfidOutOk = false;

void resetRc522Bus() {
    digitalWrite(RC522_IN_SS, HIGH);
    digitalWrite(RC522_OUT_SS, HIGH);
    digitalWrite(RC522_RST_PIN, LOW);
    delay(20);
    digitalWrite(RC522_RST_PIN, HIGH);
    delay(50);
}

bool initRc522Reader(MFRC522& reader, const char* gateLabel) {
    resetRc522Bus();
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

void scanRc522Reader(MFRC522& reader, bool readerOk, const char* gateLabel) {
    if (!readerOk) return;
    if (!reader.PICC_IsNewCardPresent()) return;
    if (!reader.PICC_ReadCardSerial()) return;

    String uidStr = "";
    for (byte index = 0; index < reader.uid.size; index++) {
        byte value = reader.uid.uidByte[index];
        if (value < 0x10) uidStr += "0";
        uidStr += String(value, HEX);
    }
    uidStr.toUpperCase();

    Serial.printf("[DIAG] %s doc duoc the UID: %s | SAK=0x%02X\n",
                  gateLabel, uidStr.c_str(), reader.uid.sak);

    reader.PICC_HaltA();
    reader.PCD_StopCrypto1();
    delay(250);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== RC522 DIAGNOSTIC MODE ===");
    Serial.println("[DIAG] Firmware test rieng cho RC522.");
    Serial.println("[DIAG] De test 1 module, chi can cam 1 RC522 duy nhat.");
    Serial.println("[DIAG] Quet the de xem UID tren Serial Monitor.");

    pinMode(RC522_IN_SS, OUTPUT);
    pinMode(RC522_OUT_SS, OUTPUT);
    pinMode(RC522_RST_PIN, OUTPUT);
    digitalWrite(RC522_IN_SS, HIGH);
    digitalWrite(RC522_OUT_SS, HIGH);
    digitalWrite(RC522_RST_PIN, HIGH);

    SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

    isRfidInOk = initRc522Reader(rfidIn, "Cong Vao");
    isRfidOutOk = initRc522Reader(rfidOut, "Cong Ra");

    Serial.printf("[DIAG] Trang thai ban dau - IN: %s, OUT: %s\n",
                  isRfidInOk ? "OK" : "FAIL",
                  isRfidOutOk ? "OK" : "FAIL");
}

void loop() {
    static uint32_t lastRetryTime = 0;
    static uint32_t lastStatusTime = 0;
    uint32_t now = millis();

    if (now - lastRetryTime > 3000) {
        lastRetryTime = now;
        if (!isRfidInOk) isRfidInOk = initRc522Reader(rfidIn, "Cong Vao");
        if (!isRfidOutOk) isRfidOutOk = initRc522Reader(rfidOut, "Cong Ra");
    }

    if (now - lastStatusTime > 5000) {
        lastStatusTime = now;
        Serial.printf("[DIAG] IN: %s | OUT: %s\n",
                      isRfidInOk ? "OK" : "FAIL",
                      isRfidOutOk ? "OK" : "FAIL");
    }

    scanRc522Reader(rfidIn, isRfidInOk, "Cong Vao");
    scanRc522Reader(rfidOut, isRfidOutOk, "Cong Ra");
    delay(30);
}
