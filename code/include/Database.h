#ifndef DATABASE_H
#define DATABASE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

struct User {
    String uid;
    String name;
    String studentId;
    String plateNumber;
    String vehicleType; // "Car" hoặc "Motorbike"
    long balance;
    String status;      // "active" hoặc "locked"
    bool inParking;     // true: đang trong bãi, false: ngoài bãi
    String entryTime;   // Thời gian vào gần nhất: "YYYY-MM-DD HH:MM:SS"
};

// Khởi tạo hệ thống tệp tin LittleFS
bool initDatabase();

// Quản lý người dùng
bool saveUser(const User& user);
bool getUser(const String& uid, User& user);
bool deleteUser(const String& uid);
void clearDatabase();

// Quản lý hàng đợi giao dịch ngoại tuyến (Offline Logs Queue)
bool queueOfflineLog(const String& uid, const String& event, long fee, long balance, const String& timeStr, bool includeAmounts = false);
bool getNextOfflineLog(String& logPayload);
bool popOfflineLog();
int getOfflineLogCount();

#endif // DATABASE_H
