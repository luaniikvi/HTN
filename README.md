# HỆ THỐNG KHÓA CỬA THÔNG MINH NHẬN DIỆN KHUÔN MẶT (ESP32-S3 & IOT DASHBOARD)

Hệ thống khóa cửa an ninh ứng dụng Edge AI nhận diện khuôn mặt cục bộ trên vi điều khiển **ESP32-S3 (N16R8 + Camera OV5640)**, tích hợp điều khiển từ xa thời gian thực qua giao thức **MQTT / WebSocket** và giao diện quản trị Web hiện đại (**Vue 3 + Vite**).

---

## MỤC LỤC
1. [Cấu trúc dự án](#1-cấu-trúc-dự-án)
2. [Yêu cầu môi trường](#2-yêu-cầu-môi-trường)
3. [Bước 1: Build & Chạy Backend](#3-bước-1-build--chạy-backend)
4. [Bước 2: Build & Chạy Frontend](#4-bước-2-build--chạy-frontend)
5. [Bước 3: Vị trí File Firmware ESP32](#5-bước-3-vị-trí-file-firmware-esp32)
6. [Bước 4: Cấu hình Arduino IDE trước khi nạp Firmware](#6-bước-4-cấu-hình-arduino-ide-trước-khi-nạp-firmware)
7. [Tài khoản mặc định & Cổng dịch vụ](#7-tài-khoản-mặc-định--cổng-dịch-vụ)

---

## 1. CẤU TRÚC DỰ ÁN

```text
HTN-1.0.1/
├── backend/               # Mã nguồn máy chủ Node.js/Express, MySQL, MQTT Broker (Docker)
│   ├── docker-compose.yml # File cấu hình khởi chạy toàn bộ dịch vụ backend
│   ├── Dockerfile
│   ├── .env               # Biến môi trường Backend
│   └── src/               # REST API, MQTT Gateway, Video Stream Relay, Database Models
├── frontend/              # Ứng dụng Web Dashboard (Vue 3, Pinia, Vite)
│   ├── package.json
│   ├── vite.config.js
│   └── src/               # Views, Components, Stores, Router
├── espOTA_FaceSecure/     # Toàn bộ mã nguồn Firmware ESP32-S3
│   ├── espOTA_FaceSecure.ino # File sketch chính mở trong Arduino IDE
│   ├── App.cpp            # Logic hệ thống, ngoại vi, FreeRTOS tasks, Camera Stream
│   ├── env.h              # Cấu hình Wi-Fi, IP Server, ngưỡng AI nhận diện khuôn mặt
│   ├── FaceEngine.h       # Engine phát hiện và trích xuất đặc trưng khuôn mặt
│   ├── FacePolicy.h       # Chính sách xác thực & quy tắc an ninh
│   ├── FaceRuntime.h      # Quản lý hàng đợi và tác vụ AI trên Core 1
│   └── partitions.csv     # Bảng phân vùng Flash 16MB cho ESP32-S3
└── README.md
```

---

## 2. YÊU CẦU MÔI TRƯỜNG

Trước khi cài đặt, hãy đảm bảo máy tính của bạn đã cài đặt các công cụ sau:
- **Node.js**: Phiên bản `>= 18.x` (kèm `npm >= 9.x`)
- **Docker & Docker Compose**: Để chạy Backend (MySQL, Mosquitto, API) một cách nhanh chóng và đồng bộ.
- **Arduino IDE**: Khuyên dùng phiên bản `2.x` (tải tại [arduino.cc](https://www.arduino.cc/en/software)).
- **Cáp USB Type-C**: Dùng để nạp firmware và cấp nguồn cho kit ESP32-S3 (hỗ trợ truyền dữ liệu).

---

## 3. BƯỚC 1: BUILD & CHẠY BACKEND

Hệ thống Backend bao gồm 3 dịch vụ được đóng gói sẵn qua Docker:
1. **MySQL 8.0**: Lưu trữ tài khoản, hồ sơ khuôn mặt (`faces`), nhật ký mở cửa (`access_logs`), nhật ký cảnh báo (`alarm_logs`), trạng thái hệ thống (`system_state`).
2. **Eclipse Mosquitto 2.x**: MQTT Broker làm cầu nối giao tiếp thời gian thực với ESP32 (cổng `1883` và WebSocket `9001`).
3. **Backend Service (Node.js)**: RESTful API (HTTP `3000`, HTTPS `3443`), WebSocket Stream Relay, UDP Auto-Discovery (`8888/udp`).

### Hướng dẫn khởi chạy bằng Docker Compose (Khuyên dùng):

1. Mở terminal, di chuyển vào thư mục `backend`:
   ```bash
   cd backend
   ```

2. Khởi chạy và build các container:
   ```bash
   docker-compose up -d --build
   ```

3. Kiểm tra các container đang chạy:
   ```bash
   docker ps
   ```
   3 container: `doorlock_backend`, `doorlock_mosquitto`, `doorlock_mysql` đều có trạng thái `Up`.

4. Xem log kiểm tra kết nối CSDL và MQTT:
   ```bash
   docker logs -f doorlock_backend
   ```
   Khi thấy dòng:
   ```text
   ✅ Database connected successfully
   👤 Synchronized admin password (admin / admin123)
   ✅ Connected to MQTT Broker
   🚀 HTTP Server & ESP32 Camera WS listening on http://0.0.0.0:3000
   🔒 HTTPS Secure Server listening on https://0.0.0.0:3443
   ```
   -> Backend đã sẵn sàng!

---

## 4. BƯỚC 2: BUILD VÀ CHẠY FRONTEND

Ứng dụng Frontend được xây dựng bằng **Vue 3** và **Vite**.

1. Mở terminal mới, di chuyển vào thư mục `frontend`:
   ```bash
   cd frontend
   ```

2. Cài đặt các thư viện dependencies:
   ```bash
   npm install
   ```

3. Chạy giao diện Web ở chế độ phát triển (Development):
   ```bash
   npm run dev
   ```
   Terminal sẽ hiển thị đường link truy cập, thông thường là:
   ```text
   ➜  Local:   http://localhost:5173/
   ```

4. Truy cập trình duyệt vào `http://localhost:5173` và đăng nhập với tài khoản:
   - **Username**: `admin`
   - **Password**: `admin123`

5. (Tùy chọn) Đóng gói ứng dụng để đưa vào vận hành (Production Build):
   ```bash
   npm run build
   ```
   Mã nguồn sau khi build sẽ nằm trong thư mục `frontend/dist/`.

---

## 5. BƯỚC 3: VỊ TRÍ FILE FIRMWARE ESP32

Toàn bộ mã nguồn firmware nằm tại thư mục:
```text
HTN-1.0.1/espOTA_FaceSecure/
```

- **File chính để mở bằng Arduino IDE**:
  👉 **`espOTA_FaceSecure/espOTA_FaceSecure.ino`**
  *(Khi mở file này trong Arduino IDE, tất cả các file mã nguồn `.cpp`, `.h` và bảng phân vùng `partitions.csv` trong cùng thư mục sẽ tự động được tải vào các tab của dự án).*

- **File cấu hình mạng & thông số AI**:
  👉 **`espOTA_FaceSecure/env.h`**
  Mở file này trước khi nạp để chỉnh sửa thông tin mạng Wi-Fi của bạn:
  ```cpp
  #define WIFI_SSID       "SSID"       // Tên Wi-Fi
  #define WIFI_PASS       "PASS"          // Mật khẩu Wi-Fi
  #define SERVER_IP       "IP"          // IP máy tính chạy Backend (xem bằng ipconfig trên Windows)
  #define MQTT_PORT       1883
  #define WS_PORT         3000
  ```

---

## 6. BƯỚC 4: CẤU HÌNH ARDUINO IDE TRƯỚC KHI NẠP FIRMWARE

Vi điều khiển sử dụng là **ESP32-S3 N16R8** (16MB Flash, 8MB Octal SPI PSRAM) kết hợp cụm Camera **OV5640**. Cần cấu hình chính xác từng mục dưới đây trong Arduino IDE để firmware hoạt động chuẩn xác và không bị lỗi tràn RAM hoặc crash:

### 4.1. Cài đặt ESP32 Board Core (Bắt buộc phiên bản 2.0.17)
> [!IMPORTANT]
> Firmware có kiểm tra phiên bản nghiêm ngặt `#if ESP_ARDUINO_VERSION != 2.0.17`. **Bắt buộc** cài đúng phiên bản **2.0.17**, không sử dụng bản 3.x để tránh xung đột thư viện `esp_camera` và nhận diện khuôn mặt.

1. Vào **File** -> **Preferences** (hoặc nhấn `Ctrl + ,`).
2. Tại mục *Additional boards manager URLs*, thêm đường dẫn:
   ```text
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Vào **Tools** -> **Board** -> **Boards Manager...**
4. Tìm từ khóa `esp32` (của Espressif Systems).
5. Tại ô lựa chọn phiên bản, chọn **`2.0.17`** rồi nhấn **Install**.

---

### 4.2. Cài đặt các thư viện cần thiết (Library Manager)
Vào **Tools** -> **Manage Libraries...** (hoặc `Ctrl + Shift + I`), tìm và cài đặt:
1. **`PubSubClient`** (bởi *Nick O'Leary*): Phiên bản mới nhất (giao tiếp MQTT).
2. **`WebSockets`** (bởi *Markus Sattler*): Phiên bản mới nhất (truyền stream camera binary JPEG).
3. **`ArduinoJson`** (bởi *Benoît Blanchon*): **Phiên bản 6.x** (ví dụ `6.21.5`). *Lưu ý: Không chọn phiên bản 7.x*.
*(Các thư viện `esp_camera`, `WiFi`, `Preferences`, `HTTPClient`, `FreeRTOS` đã được tích hợp sẵn trong esp32 core 2.0.17).*

---

### 4.3. Cấu hình thông số nạp trong menu Tools (Arduino IDE)
Mở file `espOTA_FaceSecure.ino` trong Arduino IDE và chọn cấu hình tại menu **Tools** chính xác như sau:

| Mục trong Tools | Giá trị cần chọn | Ghi chú quan trọng |
| :--- | :--- | :--- |
| **Board** | **`ESP32S3 Dev Module`** | Khung chuẩn cho chip ESP32-S3 |
| **Port** | Chọn cổng COM tương ứng | Xem trong Device Manager (vd: COM3, COM5) |
| **USB CDC On Boot** | **`Enabled`** | Giúp in log Serial ra cổng USB |
| **CPU Frequency** | **`240MHz (WiFi)`** | Đảm bảo tốc độ xử lý AI và mã hóa frame ảnh |
| **Events Run On** | **`Core 0`** | Chạy các tác vụ không liên quan đến WiFi |
| **Flash Mode** | **`QIO 80MHz`** | Tốc độ truy xuất Flash cao |
| **Flash Size** | **`16MB (128Mb)`** | Chip N16R8 có 16MB bộ nhớ Flash |
| **Arduino Run On** | **`Core 0`** | Chọn Core 0 |
| **Partition Scheme** | **`16M Flash (3MB APP/9.9MB FATFS)`** | IDE sẽ tự nhận file `partitions.csv` nằm cùng thư mục sketch |
| **PSRAM** | **`OPI PSRAM`** | **BẮT BUỘC CHỌN OPI PSRAM** (N16R8 có 8MB Octal PSRAM cho AI & Camera) |
| **Core Debug Level** | `None` hoặc `Info` | Giữ mức None để tối ưu tốc độ |
| **Upload Speed** | `921600` (hoặc `115200`) | Tốc độ nạp firmware qua cổng Serial |

---

### 4.4. Các bước tiến hành nạp Firmware
1. Kết nối ESP32-S3 với máy tính qua cổng USB.
2. Kiểm tra lại file `espOTA_FaceSecure/env.h` đã điền đúng Wi-Fi và IP Server.
3. Bấm nút **Verify (Biểu tượng dấu tích)** để biên dịch thử. Đảm bảo không có lỗi xuất hiện.
4. Bấm nút **Upload (Biểu tượng mũi tên sang phải)** để nạp code vào board.
   *(Nếu IDE báo không vào được bootloader: Nhấn giữ nút **BOOT**, bấm nhả nút **RESET/EN**, sau đó thả nút **BOOT** rồi upload lại)*.
5. Mở **Serial Monitor** (tốc độ **`115200 baud`**) để xem log khởi động:
   ```text
   [SYSTEM] Khởi động hệ thống an ninh ESP32-S3 Face Secure...
   [CAMERA] ✅ Khởi tạo Camera OV5640 thành công!
   [NET] ✅ Đã kết nối Wi-Fi thành công! IP: 192.168.1.xxx
   [MQTT] ✅ Đã kết nối Broker thành công! ClientID=dev_01
   [AI] Mô hình nhận diện khuôn mặt sẵn sàng!
   ```

---

## 7. TÀI KHOẢN MẶC ĐỊNH & CỔNG DỊCH VỤ

### Tài khoản Web Dashboard:
- **URL**: `http://localhost:5173`
- **Username**: `admin`
- **Password**: `admin123`

### Các cổng mạng (Network Ports):
| Cổng | Giao thức | Dịch vụ |
| :---: | :---: | :--- |
| **5173** | HTTP | Giao diện Web Client Frontend (Vite) |
| **3000** | HTTP / WS | RESTful API & ESP32 Camera Stream WebSocket |
| **3443** | HTTPS / WSS | Web Viewer Live Stream bảo mật SSL |
| **1883** | TCP | Mosquitto MQTT Broker |
| **3307** | TCP | Cổng ngoài của MySQL Database (truy cập từ DBeaver / Navicat) |
| **8888** | UDP | UDP Discovery Service (tự động phát hiện IP Server trong mạng LAN) |
