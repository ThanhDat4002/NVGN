#include "Database.h"

namespace {
constexpr const char* OFFLINE_LOG_PATH = "/offline_logs.json";

bool ensureOfflineLogFile() {
    if (LittleFS.exists(OFFLINE_LOG_PATH)) {
        return true;
    }

    File file = LittleFS.open(OFFLINE_LOG_PATH, "w");
    if (!file) {
        return false;
    }

    file.print("[]");
    file.close();
    return true;
}
}

// Hàm phụ để lấy đường dẫn tệp người dùng từ UID
static String getUserFilePath(const String& uid) {
    return "/u_" + uid + ".json";
}

bool initDatabase() {
    if (!LittleFS.begin(true)) {
        Serial.println("[-] Khoi tao LittleFS THAT BAI!");
        return false;
    }
    ensureOfflineLogFile();
    Serial.println("[+] Khoi tao LittleFS thanh cong.");
    return true;
}

bool saveUser(const User& user) {
    String path = getUserFilePath(user.uid);
    File file = LittleFS.open(path, "w");
    if (!file) {
        Serial.printf("[-] Khong the mo tep de ghi: %s\n", path.c_str());
        return false;
    }

    StaticJsonDocument<512> doc;
    doc["uid"] = user.uid;
    doc["name"] = user.name;
    doc["studentId"] = user.studentId;
    doc["plateNumber"] = user.plateNumber;
    doc["vehicleType"] = user.vehicleType;
    doc["balance"] = user.balance;
    doc["status"] = user.status;
    doc["inParking"] = user.inParking;
    doc["entryTime"] = user.entryTime;

    if (serializeJson(doc, file) == 0) {
        Serial.println("[-] Ghi JSON vao tep that bai!");
        file.close();
        return false;
    }

    file.close();
    Serial.printf("[+] Da luu thong tin the: %s\n", user.uid.c_str());
    return true;
}

bool getUser(const String& uid, User& user) {
    String path = getUserFilePath(uid);
    if (!LittleFS.exists(path)) {
        Serial.printf("[-] The chua duoc dang ky: %s\n", uid.c_str());
        return false;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("[-] Khong the mo tep de doc: %s\n", path.c_str());
        return false;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("[-] Giai ma JSON that bai: %s\n", error.c_str());
        return false;
    }

    user.uid = doc["uid"].as<String>();
    user.name = doc["name"].as<String>();
    user.studentId = doc["studentId"].as<String>();
    user.plateNumber = doc["plateNumber"].as<String>();
    user.vehicleType = doc["vehicleType"].as<String>();
    user.balance = doc["balance"].as<long>();
    user.status = doc["status"].as<String>();
    user.inParking = doc["inParking"].as<bool>();
    user.entryTime = doc["entryTime"].as<String>();

    return true;
}

bool deleteUser(const String& uid) {
    String path = getUserFilePath(uid);
    if (!LittleFS.exists(path)) {
        return false;
    }
    return LittleFS.remove(path);
}

void clearDatabase() {
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
        String filename = file.name();
        String userFilePath = filename;
        if (!userFilePath.startsWith("/")) {
            userFilePath = "/" + userFilePath;
        }

        if (userFilePath.startsWith("/u_") && userFilePath.endsWith(".json")) {
            file.close();
            LittleFS.remove(userFilePath);
        } else {
            file.close();
        }
        file = root.openNextFile();
    }
    LittleFS.remove(OFFLINE_LOG_PATH);
    ensureOfflineLogFile();
    Serial.println("[+] Da xoa toan bo co so du lieu cuc bo.");
}

// ========================================================
// QUẢN LÝ HÀNG ĐỢI OFFLINE LOGS (ĐỒNG BỘ SAU KHI CÓ MẠNG)
// ========================================================

bool queueOfflineLog(const String& uid, const String& event, long fee, long balance, const String& timeStr, bool includeAmounts) {
    if (!ensureOfflineLogFile()) {
        return false;
    }

    DynamicJsonDocument doc(2048);
    
    // Nếu tệp tin đã tồn tại, đọc nội dung cũ vào
    File file = LittleFS.open(OFFLINE_LOG_PATH, "r");
    if (file) {
        deserializeJson(doc, file);
        file.close();
    }
    
    // Nếu chưa có mảng, tạo mảng mới
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) {
        arr = doc.to<JsonArray>();
    }
    
    // Thêm bản ghi mới
    JsonObject newLog = arr.createNestedObject();
    newLog["uid"] = uid;
    newLog["event"] = event;
    newLog["time"] = timeStr;
    if (includeAmounts) {
        newLog["fee"] = fee;
        newLog["balance"] = balance;
    }

    // Ghi lại vào LittleFS
    file = LittleFS.open(OFFLINE_LOG_PATH, "w");
    if (!file) {
        return false;
    }
    serializeJson(doc, file);
    file.close();
    
    Serial.printf("[+] Luu log ngoai tuyen thanh cong cho UID: %s (Hien tai co %d log)\n", uid.c_str(), arr.size());
    return true;
}

bool getNextOfflineLog(String& logPayload) {
    if (!ensureOfflineLogFile()) {
        return false;
    }

    File file = LittleFS.open(OFFLINE_LOG_PATH, "r");
    if (!file) {
        return false;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull() || arr.size() == 0) {
        return false;
    }

    // Lấy phần tử đầu tiên trong hàng đợi
    serializeJson(arr[0], logPayload);
    return true;
}

bool popOfflineLog() {
    if (!ensureOfflineLogFile()) {
        return false;
    }

    File file = LittleFS.open(OFFLINE_LOG_PATH, "r");
    if (!file) {
        return false;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull() || arr.size() == 0) {
        return false;
    }

    // Xóa phần tử đầu tiên
    arr.remove(0);

    // Ghi lại tệp tin
    file = LittleFS.open(OFFLINE_LOG_PATH, "w");
    if (!file) {
        return false;
    }
    serializeJson(doc, file);
    file.close();
    return true;
}

int getOfflineLogCount() {
    if (!ensureOfflineLogFile()) {
        return 0;
    }

    File file = LittleFS.open(OFFLINE_LOG_PATH, "r");
    if (!file) {
        return 0;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        return 0;
    }

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) {
        return 0;
    }
    return arr.size();
}
