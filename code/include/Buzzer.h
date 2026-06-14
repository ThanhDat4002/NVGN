#ifndef BUZZER_H
#define BUZZER_H

#include "Config.h"

// Khởi tạo chân còi
void initBuzzer();

// Phát âm thanh tần số tùy chọn trong khoảng thời gian nhất định (ms)
void playTone(int frequency, int durationMs);

// Kêu tiếng beep ngắn (thành công)
void beepSuccess();

// Kêu tiếng beep kéo dài / báo lỗi (thất bại)
void beepError();

// Kêu tiếng cảnh báo
void beepWarning();

#endif // BUZZER_H
