#include "Buzzer.h"

// Hàm điều khiển còi chủ động: chỉ bật/tắt mức điện áp theo thời gian mong muốn.
void playTone(int frequency, int durationMs) {
    (void)frequency;

#if BUZZER_ACTIVE_HIGH
    digitalWrite(BUZZER_PIN, HIGH);
    delay(durationMs);
    digitalWrite(BUZZER_PIN, LOW);
#else
    digitalWrite(BUZZER_PIN, LOW);
    delay(durationMs);
    digitalWrite(BUZZER_PIN, HIGH);
#endif
}

void initBuzzer() {
    pinMode(BUZZER_PIN, OUTPUT);
#if BUZZER_ACTIVE_HIGH
    digitalWrite(BUZZER_PIN, LOW);
#else
    digitalWrite(BUZZER_PIN, HIGH);
#endif
}

void beepSuccess() {
    // 1 tiếng bíp ngắn báo hiệu quét thẻ thành công
    playTone(2000, 100);
}

void beepError() {
    // 2 tiếng bíp ngắn báo hiệu lỗi thẻ / không nhận thẻ
    for (int i = 0; i < 2; i++) {
        playTone(1000, 120);
        if (i < 1) delay(80);
    }
}

void beepWarning() {
    // 3 tiếng bíp báo hiệu cảnh báo mức cao, ví dụ không đủ tiền
    for (int i = 0; i < 3; i++) {
        playTone(1200, 100);
        if (i < 2) delay(80);
    }
}
