#pragma once

// =================================================================================================
// CẤU HÌNH HỆ THỐNG ESP32-S3 FACE SECURE (env.h / .env)
// =================================================================================================

// 1. CẤU HÌNH WIFI & MÁY CHỦ (NETWORK)
#define WIFI_SSID               "Nguyen Loi"
#define WIFI_PASS               "123456NL"
#define SERVER_IP               "192.168.1.188"
#define MQTT_PORT               1883
#define WS_PORT                 3000

// 2. CẤU HÌNH GÓC ĐẶT CAMERA & XỬ LÝ ẢNH
// 0: Camera đặt đứng thông thường (ảnh ngang 320x240)
// 90: Camera đặt nằm ngang (xoay 90 độ theo chiều kim đồng hồ -> ảnh dọc 240x320)
// 270: Camera đặt nằm ngang (xoay 90 độ ngược chiều kim đồng hồ)
#define ROTATION_DEGREE         90

// 3. CẤU HÌNH KHUÔN MẶT (FACE RECOGNITION & ENROLLMENT)
#define MAX_FACE                10       // Số khuôn mặt tối đa lưu trong NVS flash
#define CONFIRM_FRAMES          2        // Số frame liên tiếp nhận diện đúng trước khi kích hoạt mở cửa
#define YAW_ABS                 0.48f    // Ngưỡng góc nghiêng đầu tối đa cho phép (|yaw|)
#define MATCH_THRESHOLD         0.62f    // Ngưỡng Cosine similarity khớp khuôn mặt (>= 0.62)
#define SUPPORT_THRESHOLD       0.55f    // Ngưỡng support vector mẫu phụ (>= 0.55)

// 4. CẤU HÌNH DEBUG SERIAL
#define NET_DIAG_DEBUG          false     // In log chẩn đoán mạng WiFi & MQTT định kỳ [NET-DIAG]
#define WS_DEBUG                true     // In log sự kiện kết nối WebSocket camera stream [WS CAMERA]
#define PERF_DEBUG              false     // In log hiệu năng FPS, băng thông mạng và RAM định kỳ [PERF]
#define FACE_DEBUG              true     // In log chi tiết phát hiện và nhận diện khuôn mặt [FACE]
#define BLE_DEBUG               true     // In log trạng thái bật/tắt và nhận diện gói tin BLE iBeacon

// 5. CẤU HÌNH DEMO / KIỂM THỬ CẢM BIẾN CỬA
#define ALWAYS_DOOR_CLOSE_DEMO  true     // true: Giả lập cửa luôn đóng; false: Đọc cảm biến thật MC-38 (chân 21)

// 6. CẤU HÌNH BLUETOOTH DỰ PHÒNG (BLE iBeacon Fallback)
#define ENABLE_BLE_FALLBACK     true     // Bật BLE dự phòng khi mất kết nối Wi-Fi hoặc Server
#define DISARMED_BLE_HEX        "0215fda50693a4e24fb1afcfc6eb07647825000a0001c5"
#define ARMED_BLE_HEX           "0215fda50693a4e24fb1afcfc6eb07647825000a0002c5"
#define STAY_BLE_HEX            "0215fda50693a4e24fb1afcfc6eb07647825000a0003c5"




