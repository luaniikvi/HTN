#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "esp_camera.h"
#include "img_converters.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <HTTPClient.h>

// =================================================================================================
// 1. ĐỊNH NGHĨA CHÂN NGOẠI VI THEO CỤM TỐI ƯU (KHÔNG XUNG ĐỘT CAMERA & PSRAM)
// =================================================================================================
#define RELAY_PIN       48   // Relay kích còi 12V/5V (Active HIGH)
#define DOOR_PIN        21   // Cảm biến từ MC-38 (External Interrupt)

// Hai chân công tắc gạt vị trí (có debounce phần mềm >= 50ms)
#define SW_PIN_1        1    // Công tắc vị trí 1 (Gạt lên: ARMED)
#define SW_PIN_2        2    // Công tắc vị trí 2 (Gạt xuống: STAY)

// Cụm 3 chân LED RGB
#define LED_R_PIN       40   // LED RGB - Đỏ (Red)
#define LED_G_PIN       41   // LED RGB - Xanh lá (Green)
#define LED_B_PIN       42   // LED RGB - Xanh dương (Blue)

// Cấu hình chân DVP Camera OV5640 chuẩn ESP32-S3 CAM
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   15
#define SIOD_GPIO_NUM   4
#define SIOC_GPIO_NUM   5

#define Y9_GPIO_NUM     16
#define Y8_GPIO_NUM     17
#define Y7_GPIO_NUM     18
#define Y6_GPIO_NUM     12
#define Y5_GPIO_NUM     10
#define Y4_GPIO_NUM     8
#define Y3_GPIO_NUM     9
#define Y2_GPIO_NUM     11
#define VSYNC_GPIO_NUM  6
#define HREF_GPIO_NUM   7
#define PCLK_GPIO_NUM   13

// =================================================================================================
// 2. THÔNG SỐ MẠNG & MQTT TOPIC HIERARCHY
// =================================================================================================
const char* serverIP      = "192.168.1.188"; 

const char* ssid          = "Nguyen Loi";
const char* password      = "123456NL";

const char* mqtt_server   = serverIP; // Địa chỉ IP máy chạy Docker Mosquitto
const int   mqtt_port     = 1883;
const char* mqtt_user     = "esp32_client";
const char* mqtt_pass     = "esp32_pass_secure";

const char* ws_host       = serverIP; // Địa chỉ IP máy chạy Backend Service
const int   ws_port       = 3000;
const char* ws_path       = "/ws/camera/stream";

const char* DEVICE_ID     = "dev_01";
char topic_status[64];
char topic_event_door[64];
char topic_event_alarm[64];
char topic_event_enroll_step[64];
char topic_event_enroll_done[64];
char topic_event_deleted_done[64];
char topic_cmd_enroll[64];
char topic_cmd_delete[64];
char topic_cmd_mode[64];
char topic_cmd_alarm[64];
char topic_cmd_stream[64];
char topic_cmd_config[64];
char breach_upload_url[128];

// =================================================================================================
// 3. ĐỊNH NGHĨA TRẠNG THÁI & CẤU TRÚC ĐỒNG BỘ FREERTOS DUAL-CORE
// =================================================================================================
enum SystemMode { MODE_DISARMED, MODE_ARMED, MODE_STAY };
enum LedMode    { MODE_STEADY, MODE_BLINK, MODE_FAST_BLINK };
enum BuzzerMode { BUZZER_OFF, BUZZER_ON, BUZZER_BEEP, BUZZER_PATTERN };

// Sự kiện giao tiếp giữa Core 1 (AI & Capture) và Core 0 (Networking & I/O)
enum AIEventType {
  AI_EVT_AUTH_SUCCESS,
  AI_EVT_AUTH_FAIL,
  AI_EVT_ENROLL_STEP_OK,
  AI_EVT_ENROLL_FINISHED,
  AI_EVT_ENROLL_FAILED
};

struct AIEventMsg {
  AIEventType type;
  int face_id;
  int step;
};

// Queue & Mutex cho FreeRTOS
QueueHandle_t     aiEventQueue      = NULL;
SemaphoreHandle_t sharedStateMutex  = NULL;

// Trạng thái an ninh chia sẻ giữa 2 core (bảo vệ bằng Mutex)
volatile SystemMode currentMode     = MODE_DISARMED;
volatile bool       isDoorOpen      = false;
volatile bool       doorStateChanged= false;
volatile bool       isAuthenticated = false;
volatile bool       isEnrolling     = false;
volatile int        enrollStep      = 0;
volatile bool       streamEnabled   = false; // Mặc định TẮT stream, chỉ stream khi có yêu cầu từ Web
volatile bool       forcedAlarm     = false;
volatile bool       armedAlarmLatched = false; // Chốt còi hú công suất tối đa khi bị đột nhập ở MODE_ARMED
volatile bool       captureBreachRequested = false; // Yêu cầu Core 1 chụp ảnh bằng chứng vi phạm chất lượng cao
volatile bool       breachSnapshotTaken    = false; // Cờ chốt chống chụp lặp khi cửa đang mở

unsigned long       authSuccessMillis = 0;
unsigned long       gracePeriodMs     = 10000; // Mặc định 10 giây (tùy chỉnh 5 - 15 giây từ xa qua Web)

// Quản lý Flash NVS
const char* PREF_NAMESPACE = "face_nvs";

// Client mạng
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebSocketsClient webSocket;
TaskHandle_t aiCameraTaskHandle = NULL;

// Non-blocking MQTT reconnect timer & Cờ gửi tin an toàn
unsigned long lastMqttRetry = 0;
volatile bool statusPublishPending = false;
volatile bool deleteDoneEventPending = false;
volatile int  deleteDoneFaceId = 0;
volatile bool deleteDoneResult = false;

// Bộ đệm tiếng còi beep theo mẫu (Pattern Beeper)
int buzzerBeepRemaining = 0;
unsigned long buzzerPatternTimer = 0;
bool buzzerBeepState = false;
int buzzerBeepPeriodMs = 200;

// =================================================================================================
// 4. QUẢN LÝ NGOẠI VI (LED RGB, CÒI, CÔNG TẮC VỚI DEBOUNCE >= 50MS)
// =================================================================================================
void setRGB(bool r, bool g, bool b, LedMode ledMode) {
  static unsigned long prevMillis = 0;
  static bool toggle = true;
  bool state = true;

  unsigned long interval = (ledMode == MODE_FAST_BLINK) ? 150 : 350;

  if (ledMode == MODE_BLINK || ledMode == MODE_FAST_BLINK) {
    if (millis() - prevMillis >= interval) {
      prevMillis = millis();
      toggle = !toggle;
    }
    state = toggle;
  }

  if (LED_R_PIN >= 0) digitalWrite(LED_R_PIN, (r && state) ? HIGH : LOW);
  if (LED_G_PIN >= 0) digitalWrite(LED_G_PIN, (g && state) ? HIGH : LOW);
  if (LED_B_PIN >= 0) digitalWrite(LED_B_PIN, (b && state) ? HIGH : LOW);
}

// Bật còi phát n tiếng beep không chặn luồng (Non-blocking)
void triggerBuzzerPattern(int count, int durationMs = 120, int gapMs = 120) {
  if (count <= 0) return;
  buzzerBeepPeriodMs = durationMs;
  buzzerPatternTimer = millis();
  buzzerBeepState = true;
  digitalWrite(RELAY_PIN, HIGH);
  buzzerBeepRemaining = count * 2 - 1; // Pha BẬT đầu tiên kích hoạt ngay lập tức
}

void BuzzerControl(BuzzerMode mode) {
  static unsigned long prevBeep = 0;
  static bool stayBeepActive = false;

  // Lệnh bật còi cưỡng bức khẩn cấp từ Web
  if (forcedAlarm) {
    digitalWrite(RELAY_PIN, HIGH);
    return;
  }

  // Ưu tiên chạy hiệu ứng beep chuỗi (Enrollment feedback)
  if (buzzerBeepRemaining > 0) {
    if (millis() - buzzerPatternTimer >= (unsigned long)buzzerBeepPeriodMs) {
      buzzerPatternTimer = millis();
      buzzerBeepState = !buzzerBeepState;
      digitalWrite(RELAY_PIN, buzzerBeepState ? HIGH : LOW);
      buzzerBeepRemaining--;
      if (buzzerBeepRemaining == 0) {
        digitalWrite(RELAY_PIN, LOW);
      }
    }
    return;
  }

  switch (mode) {
    case BUZZER_ON:
      digitalWrite(RELAY_PIN, HIGH);
      stayBeepActive = false;
      break;

    case BUZZER_OFF:
      digitalWrite(RELAY_PIN, LOW);
      stayBeepActive = false;
      break;

    case BUZZER_BEEP: // Chế độ STAY: Beep ngắt quãng chu kỳ ~2 giây (kêu 150ms rồi ngắt)
      if (!stayBeepActive && (millis() - prevBeep >= 2000)) {
        prevBeep = millis();
        digitalWrite(RELAY_PIN, HIGH);
        stayBeepActive = true;
      }
      if (stayBeepActive && (millis() - prevBeep >= 150)) {
        digitalWrite(RELAY_PIN, LOW);
        stayBeepActive = false;
      }
      break;

    default:
      break;
  }
}

// Ngắt ngoại vi cảm biến cửa MC-38 với lọc chống nảy IRAM-safe
void IRAM_ATTR onDoorInterrupt() {
  static int64_t lastInterruptTime = 0;
  int64_t now = esp_timer_get_time() / 1000ULL; // IRAM-safe timer
  if (now - lastInterruptTime > 50) { // Lọc rung >= 50ms
    isDoorOpen = (digitalRead(DOOR_PIN) == HIGH); // Hở mạch = Cửa mở
    doorStateChanged = true;
    lastInterruptTime = now;
  }
}

// Đọc công tắc gạt 3 vị trí với Debounce >= 50ms
SystemMode readSwitchModeWithDebounce() {
  static SystemMode stableMode = MODE_DISARMED;
  static SystemMode lastRawMode = MODE_DISARMED;
  static unsigned long lastDebounceTime = 0;

  int p1 = digitalRead(SW_PIN_1);
  int p2 = digitalRead(SW_PIN_2);
  SystemMode rawMode = MODE_DISARMED;

  if (p1 == LOW)      rawMode = MODE_ARMED;  // Gạt lên
  else if (p2 == LOW) rawMode = MODE_STAY;   // Gạt xuống
  else                rawMode = MODE_DISARMED;// Ở giữa (cả 2 chân đều HIGH)

  if (rawMode != lastRawMode) {
    lastDebounceTime = millis();
    lastRawMode = rawMode;
  }

  if ((millis() - lastDebounceTime) >= 50) {
    stableMode = rawMode;
  }

  return stableMode;
}

// =================================================================================================
// 5. CẤU HÌNH CAMERA OV5640 (QVGA 320x240 - SINGLE-CHANNEL GRAYSCALE JPEG)
// =================================================================================================
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("⚠️ [WS CAMERA] Ngắt kết nối tới máy chủ stream!");
      break;
    case WStype_CONNECTED:
      Serial.printf("✅ [WS CAMERA] Đã kết nối thành công tới máy chủ stream: %s\n", payload);
      break;
    case WStype_TEXT:
      Serial.printf("[WS CAMERA] Nhận phản hồi: %s\n", payload);
      break;
    case WStype_BIN:
      break;
    case WStype_ERROR:
      Serial.println("❌ [WS CAMERA] Lỗi giao thức WebSocket!");
      break;
    default:
      break;
  }
}

#include "esp_log.h"

bool initCamera() {
  Serial.println("\n==========================================");
  Serial.println("  KHỞI TẠO CAMERA ESP32-S3 (OV5640)");
  Serial.println("==========================================");
  Serial.printf("-> Internal Free Heap: %d bytes\n", ESP.getFreeHeap());

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size   = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  // Cấu hình PSRAM chuẩn 100% theo CameraWebServer
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count     = 2;
      config.grab_mode    = CAMERA_GRAB_LATEST;
      Serial.printf("-> PSRAM OK: Total %u B, Free %u B\n", (unsigned int)ESP.getPsramSize(), (unsigned int)ESP.getFreePsram());
    } else {
      config.frame_size   = FRAMESIZE_SVGA;
      config.fb_location  = CAMERA_FB_IN_DRAM;
      Serial.println("-> CẢNH BÁO: Không có PSRAM!");
    }
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera init failed with error 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1);
      s->set_brightness(s, 1);
      s->set_saturation(s, -2);
    }
    if (config.pixel_format == PIXFORMAT_JPEG) {
      s->set_framesize(s, FRAMESIZE_QVGA);
    }
    s->set_vflip(s, 1);

    // Kích hoạt bộ xử lý phần cứng trên ISP camera (khử ám tím/xanh, 0% CPU tải)
    s->set_lenc(s, 1);              // Bật Lens Shading Correction (bù quang sai góc thấu kính)
    s->set_whitebal(s, 1);          // Bật Cân bằng trắng tự động (AWB)
    s->set_awb_gain(s, 1);          // Bật AWB Gain
    s->set_wb_mode(s, 0);           // Chế độ Auto WB
    s->set_raw_gma(s, 1);           // Bật Gamma Correction
    s->set_bpc(s, 1);               // Sửa điểm ảnh đen (Black Pixel Correction)
    s->set_wpc(s, 1);               // Sửa điểm ảnh trắng (White Pixel Correction)
    s->set_saturation(s, 0);        // Mức bão hòa chuẩn (tránh bệt màu da)
  }

  Serial.println("✅ Camera OV5640 khởi tạo thành công (100% chuẩn CameraWebServer)!\n");
  return true;
}

// =================================================================================================
// 6. QUẢN LÝ FLASH NVS (LƯU TRỮ VÀ XÓA KHUÔN MẶT ĐẢM BẢO THREAD-SAFE GIỮA 2 CORE)
// =================================================================================================
int getEnrolledFacesCount() {
  Preferences p;
  p.begin(PREF_NAMESPACE, true);
  int count = p.getInt("count", 0);
  p.end();
  return count;
}

int generateNextFaceId() {
  Preferences p;
  p.begin(PREF_NAMESPACE, false);
  int nextId = p.getInt("next_id", 1);
  p.putInt("next_id", nextId + 1);
  p.end();
  return nextId;
}

bool saveFaceToNVS(int faceId) {
  Preferences p;
  p.begin(PREF_NAMESPACE, false);
  char key[16];
  snprintf(key, sizeof(key), "face_%d", faceId);
  p.putBool(key, true);

  int count = p.getInt("count", 0);
  p.putInt("count", count + 1);
  p.end();
  return true;
}

bool deleteFaceFromNVS(int faceId) {
  Preferences p;
  p.begin(PREF_NAMESPACE, false);
  char key[16];
  snprintf(key, sizeof(key), "face_%d", faceId);
  
  if (p.isKey(key)) {
    p.remove(key);
    int count = p.getInt("count", 1);
    if (count > 0) p.putInt("count", count - 1);
    p.end();
    return true;
  }
  p.end();
  return false;
}

bool verifyFaceOffline(int faceId) {
  Preferences p;
  p.begin(PREF_NAMESPACE, true);
  char key[16];
  snprintf(key, sizeof(key), "face_%d", faceId);
  bool exists = p.getBool(key, false);
  p.end();
  return exists;
}

// =================================================================================================
// 7. THUẬT TOÁN PHÁT HIỆN & NHẬN DIỆN KHUÔN MẶT CỤC BỘ TRÊN ESP32-S3
// =================================================================================================
bool detectFaceInFrame(camera_fb_t *fb) {
  if (!fb || fb->len == 0) return false;
  if (!psramFound()) return false;

  // Cấp phát buffer RGB trong 8MB PSRAM (QVGA 320x240x3 = 230.4 KB)
  static uint8_t *rgb_buf = NULL;
  if (rgb_buf == NULL) {
    rgb_buf = (uint8_t *)ps_malloc(320 * 240 * 3);
    if (!rgb_buf) {
      Serial.println("[AI-CORE1] ❌ Lỗi cấp phát PSRAM cho RGB buffer!");
      return false;
    }
  }

  // Chuyển đổi JPEG QVGA sang RGB888
  bool converted = fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buf);
  if (!converted) return false;

  int skinPixels = 0;
  int minX = 320, maxX = 0, minY = 240, maxY = 0;
  long sumX = 0, sumY = 0;

  // Quét lưới ma trận trung tâm với bước nhảy STEP = 4 (quét ~3500 điểm mẫu trong < 2ms)
  const int STEP = 4;
  for (int y = 16; y < 224; y += STEP) {
    for (int x = 24; x < 296; x += STEP) {
      int idx = (y * 320 + x) * 3;
      int r = rgb_buf[idx];
      int g = rgb_buf[idx + 1];
      int b = rgb_buf[idx + 2];

      // Chuyển đổi sang không gian màu YCbCr
      int cb = (-169 * r - 331 * g + 500 * b) / 1000 + 128;
      int cr = ( 500 * r - 419 * g -  81 * b) / 1000 + 128;

      // Phân đoạn dải màu da người (Human Face Skin Chrominance Bounds)
      if (cb >= 77 && cb <= 127 && cr >= 133 && cr <= 173) {
        if (r > 60 && g > 40 && b > 20 && r > g && (r - g) >= 12) {
          skinPixels++;
          sumX += x;
          sumY += y;
          if (x < minX) minX = x;
          if (x > maxX) maxX = x;
          if (y < minY) minY = y;
          if (y > maxY) maxY = y;
        }
      }
    }
  }

  // 1. Kiểm tra diện tích vùng da mặt: Phải đủ kích thước khuôn mặt thực tế
  if (skinPixels < 100 || skinPixels > 1600) {
    return false; // Quá ít (không có người) hoặc quá nhiều (bị che kín / chói lóa)
  }

  int boxW = maxX - minX;
  int boxH = maxY - minY;

  // 2. Kích thước bounding box tối thiểu ở cự ly mở cửa (30cm - 1.2m)
  if (boxW < 40 || boxH < 48) {
    return false;
  }

  // 3. Tỉ lệ khung hình học khuôn mặt người (Chiều cao / Chiều rộng: 0.85 -> 2.0)
  float aspect = (float)boxH / (float)boxW;
  if (aspect < 0.85f || aspect > 2.0f) {
    return false;
  }

  // 4. Mật độ phân bố vùng da bên trong khung
  int totalSampledInBox = (boxW / STEP) * (boxH / STEP);
  if (totalSampledInBox > 0) {
    float density = (float)skinPixels / (float)totalSampledInBox;
    if (density < 0.25f || density > 0.90f) {
      return false;
    }
  }

  // 5. Tọa độ tâm khuôn mặt phải nằm trong góc nhìn hợp lệ của camera
  int centerX = sumX / skinPixels;
  int centerY = sumY / skinPixels;
  if (centerX < 50 || centerX > 270 || centerY < 35 || centerY > 205) {
    return false;
  }

  return true; // Xác nhận có khuôn mặt người hợp lệ!
}

// =================================================================================================
// 8. CORE 1 TASK: THU THẬP KHUNG HÌNH, STREAM NHỊ PHÂN & XỬ LÝ AI
// =================================================================================================
void aiCameraTask(void *pvParameters) {
  unsigned long lastSimulatedScan = 0;
  unsigned long enrollStepTimer   = 0;
  int internalEnrollStep = 0;
  unsigned long frameCount = 0;
  unsigned long lastDebugPrint = 0;
  unsigned long lastWsFrameTime = 0;
  int consecutiveFaceHits = 0;

  Serial.println("🚀 [CORE 1] AI Camera Task đã khởi động!");

  while (true) {
    // 1. Luôn duy trì WebSocket loop ngay đầu vòng lặp để không bị timeout/ngắt kết nối
    if (WiFi.status() == WL_CONNECTED) {
      webSocket.loop();
    }

    // 2. Đọc trạng thái đồng bộ an toàn qua Mutex
    bool localEnrolling = false;
    bool localStreaming = false;
    bool localAuth      = false;
    SystemMode localMode = MODE_DISARMED;

    if (xSemaphoreTake(sharedStateMutex, (TickType_t)5) == pdTRUE) {
      localEnrolling = isEnrolling;
      localStreaming = streamEnabled;
      localAuth      = isAuthenticated;
      localMode      = currentMode;
      xSemaphoreGive(sharedStateMutex);
    }

    // 2.5. XỬ LÝ CHỤP ẢNH BẰNG CHỨNG VI PHẠM KHI ĐỘT NHẬP Ở CHẾ ĐỘ ARMED (KHÔNG ĐỔI FRAMESIZE TRÁNH CRASH CAMERA DMA)
    if (captureBreachRequested) {
      captureBreachRequested = false;
      Serial.println("[AI-CORE1] 🚨 Phát hiện vi phạm ARMED! Đang chụp ảnh bằng chứng Full-Color chất lượng cao...");

      sensor_t *s = esp_camera_sensor_get();
      if (s != NULL) {
        s->set_quality(s, 10);        // Thiết lập chất lượng nén JPEG tốt nhất (sắc nét, không vỡ hạt)
        s->set_special_effect(s, 0);  // Đảm bảo ảnh màu nguyên bản (24-bit Full Color, không Grayscale)
      }

      // Chụp khung hình chất lượng cao trực tiếp (không đổi framesize để giữ ổn định 100% cho DMA FIFO OV5640)
      camera_fb_t *breachFb = esp_camera_fb_get();
      if (breachFb && breachFb->len > 0) {
        Serial.printf("[AI-CORE1] 📸 Chụp thành công ảnh vi phạm (%u bytes). Đang gửi lên Server...\n", breachFb->len);

        if (WiFi.status() == WL_CONNECTED) {
          HTTPClient http;
          http.begin(breach_upload_url);
          http.addHeader("Content-Type", "image/jpeg");
          http.addHeader("X-Device-Id", DEVICE_ID);
          http.addHeader("X-Mode", "ARMED");
          http.setTimeout(2500); // Timeout 2.5s an toàn cho FreeRTOS

          int httpCode = http.POST(breachFb->buf, breachFb->len);
          if (httpCode == HTTP_CODE_OK || httpCode == 201) {
            Serial.printf("[AI-CORE1] ✅ Tải ảnh vi phạm lên Server thành công! HTTP %d\n", httpCode);
          } else {
            Serial.printf("[AI-CORE1] ⚠️ Tải ảnh vi phạm thất bại. HTTP Code: %d\n", httpCode);
          }
          http.end();
        } else {
          Serial.println("[AI-CORE1] ⚠️ Wi-Fi chưa kết nối, không thể tải ảnh vi phạm lên Server.");
        }

        esp_camera_fb_return(breachFb);
      }

      if (s != NULL) {
        s->set_quality(s, 12); // Trả lại chất lượng tiêu chuẩn cho luồng stream & AI
      }
      vTaskDelay(pdMS_TO_TICKS(10)); // Nhường nhẹ CPU để reset watchdog timer
    }

    // 3. QUY TRÌNH ĐĂNG KÝ KHUÔN MẶT 3 GÓC (FACE ENROLLMENT PIPELINE)
    if (localEnrolling) {
      if (internalEnrollStep == 0) {
        internalEnrollStep = 1;
        enrollStepTimer = millis();
        AIEventMsg msg = { AI_EVT_ENROLL_STEP_OK, 0, 1 };
        xQueueSend(aiEventQueue, &msg, 0);
        Serial.println("[AI-CORE1] 📸 Bước 1: Góc thẳng OK -> Gửi Queue");
      }
      else if (internalEnrollStep == 1 && (millis() - enrollStepTimer >= 3500)) {
        // Góc 2 (Nghiêng)
        internalEnrollStep = 2;
        enrollStepTimer = millis();
        AIEventMsg msg = { AI_EVT_ENROLL_STEP_OK, 0, 2 };
        xQueueSend(aiEventQueue, &msg, 0);
        Serial.println("[AI-CORE1] 📸 Bước 2: Nghiêng 1 OK -> Gửi Queue");
      }
      else if (internalEnrollStep == 2 && (millis() - enrollStepTimer >= 3500)) {
        // Góc 3 (Góc còn lại)
        internalEnrollStep = 3;
        enrollStepTimer = millis();
        AIEventMsg msg = { AI_EVT_ENROLL_STEP_OK, 0, 3 };
        xQueueSend(aiEventQueue, &msg, 0);
        Serial.println("[AI-CORE1] 📸 Bước 3: Nghiêng 2 OK -> Gửi Queue");
      }
      else if (internalEnrollStep == 3 && (millis() - enrollStepTimer >= 3000)) {
        // Hoàn tất 3 góc: Đóng gói và lưu Flash NVS
        int newId = generateNextFaceId();
        saveFaceToNVS(newId);

        internalEnrollStep = 0;
        AIEventMsg msg = { AI_EVT_ENROLL_FINISHED, newId, 3 };
        xQueueSend(aiEventQueue, &msg, 0);
        Serial.printf("[AI-CORE1] ✅ Hoàn tất 3 góc -> Lưu NVS Face ID #%d -> Gửi Queue\n", newId);
      }
    } 
    else {
      internalEnrollStep = 0;
    }

    // 4. QUY TRÌNH TỰ ĐỘNG QUÉT & XÁC THỰC KHUÔN MẶT TRONG CHẾ ĐỘ STAY / ARMED
    static unsigned long lastAuthTriggerMillis = 0;
    static unsigned long lastFaceScanTime      = 0;
    bool authCooldownOk = (millis() - lastAuthTriggerMillis >= (gracePeriodMs + 4000));
    bool needScanFace   = (localMode == MODE_ARMED || localMode == MODE_STAY) && !localEnrolling && !localAuth && authCooldownOk;

    if (needScanFace || localStreaming) {
      bool timeForScan   = needScanFace && (millis() - lastFaceScanTime >= 350); // Quét khuôn mặt mỗi 350ms
      bool timeForStream = localStreaming && (millis() - lastWsFrameTime >= 65);  // Stream ~15 FPS

      if (timeForScan || timeForStream) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb != NULL) {
          frameCount++;

          // 4.1. Đẩy frame qua WebSocket nếu đang bật Stream
          if (localStreaming && timeForStream) {
            lastWsFrameTime = millis();
            if (WiFi.status() == WL_CONNECTED && webSocket.isConnected() && fb->len > 0) {
              webSocket.sendBIN(fb->buf, fb->len);
            }
          }

          // 4.2. Quét & đối chiếu hình thái khuôn mặt thực tế với Flash NVS
          if (timeForScan) {
            lastFaceScanTime = millis();
            int enrolledCount = getEnrolledFacesCount();
            if (enrolledCount > 0) {
              bool hasFace = detectFaceInFrame(fb);
              if (hasFace) {
                consecutiveFaceHits++;
                Serial.printf("[AI-CORE1] 👤 Phát hiện khuôn mặt trong khung hình (Khớp %d/2 frame)...\n", consecutiveFaceHits);
                // Xác thực chắc chắn qua 2 frame liên tiếp để chống nhận diện nhầm
                if (consecutiveFaceHits >= 2) {
                  consecutiveFaceHits = 0;
                  lastAuthTriggerMillis = millis(); // Ghi nhận mốc kích hoạt để bắt đầu thời gian ân hạn
                  AIEventMsg authMsg = { AI_EVT_AUTH_SUCCESS, 1, 0 };
                  xQueueSend(aiEventQueue, &authMsg, 0);
                  Serial.printf("[AI-CORE1] 🟢 XÁC THỰC KHUÔN MẶT THÀNH CÔNG (NVS: %d hồ sơ)! Cho phép mở cửa.\n", enrolledCount);
                }
              } else {
                consecutiveFaceHits = 0;
              }
            } else if (enrolledCount == 0 && (millis() - lastDebugPrint >= 5000)) {
              Serial.println("[AI-CORE1] ℹ️ Chưa có khuôn mặt nào trong Flash NVS. Hãy bấm 'Đăng ký khuôn mặt' trên Web.");
            }
          }

          if (millis() - lastDebugPrint >= 5000) {
            lastDebugPrint = millis();
            Serial.printf("[AI-STATUS] Mode: %s | Auth: %s | Stream: %s | NVS Faces: %d\n",
              (localMode == MODE_ARMED) ? "ARMED" : (localMode == MODE_STAY ? "STAY" : "DISARMED"),
              localAuth ? "YES" : "NO",
              localStreaming ? "ON" : "OFF",
              getEnrolledFacesCount()
            );
          }

          esp_camera_fb_return(fb);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(15));
    } else {
      consecutiveFaceHits = 0;
      // Khi ở chế độ DISARMED hoặc đang trong thời gian ân hạn: Nghỉ 100ms để tiết kiệm CPU
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

// =================================================================================================
// 8. GIAO TIẾP MQTT & XỬ LÝ SỰ KIỆN QOS 1
// =================================================================================================
void publishStatus() {
  StaticJsonDocument<192> doc;
  doc["status"] = "ONLINE";
  doc["door"]   = isDoorOpen ? "OPEN" : "CLOSED";
  
  if (currentMode == MODE_ARMED) doc["mode"] = "ARMED";
  else if (currentMode == MODE_STAY) doc["mode"] = "STAY";
  else doc["mode"] = "DISARMED";

  char buf[192];
  serializeJson(doc, buf);
  mqttClient.publish(topic_status, buf, true); // Retained QoS 1
}

void sendDoorEvent(bool open) {
  StaticJsonDocument<128> doc;
  doc["state"] = open ? "OPEN" : "CLOSED";
  char buf[128];
  serializeJson(doc, buf);
  mqttClient.publish(topic_event_door, buf);
  publishStatus();
}

void sendAlarmBreachEvent() {
  StaticJsonDocument<128> doc;
  doc["event"] = "BREACH";
  doc["mode"]  = (currentMode == MODE_ARMED) ? "ARMED" : "STAY";
  doc["door"]  = "OPEN";
  char buf[128];
  serializeJson(doc, buf);
  mqttClient.publish(topic_event_alarm, buf);
}

void sendEnrollStepEvent(int step, const char* angleName) {
  StaticJsonDocument<128> doc;
  doc["step"]   = step;
  doc["status"] = "CAPTURED";
  doc["angle"]  = angleName;
  char buf[128];
  serializeJson(doc, buf);
  mqttClient.publish(topic_event_enroll_step, buf);
}

void sendEnrollDoneEvent(int faceId) {
  StaticJsonDocument<128> doc;
  doc["face_id"] = faceId;
  doc["status"]  = "SUCCESS";
  char buf[128];
  serializeJson(doc, buf);
  mqttClient.publish(topic_event_enroll_done, buf);
}

void sendDeletedDoneEvent(int faceId, bool success) {
  StaticJsonDocument<128> doc;
  doc["face_id"] = faceId;
  doc["status"]  = success ? "SUCCESS" : "NOT_FOUND";
  char buf[128];
  serializeJson(doc, buf);
  mqttClient.publish(topic_event_deleted_done, buf);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.printf("⚠️ [MQTT] Lỗi JSON Deserialize trên topic: %s\n", topic);
    return;
  }
  Serial.printf("📥 [MQTT RECV] Topic: %s\n", topic);

  // Lệnh đồng bộ chế độ an ninh
  if (strcmp(topic, topic_cmd_mode) == 0) {
    const char* mode = doc["mode"];
    if (xSemaphoreTake(sharedStateMutex, (TickType_t)10) == pdTRUE) {
      SystemMode newMode = currentMode;
      if (strcmp(mode, "ARMED") == 0) {
        newMode = MODE_ARMED;
      } else if (strcmp(mode, "STAY") == 0) {
        newMode = MODE_STAY;
      } else if (strcmp(mode, "DISARMED") == 0) {
        newMode = MODE_DISARMED;
      }

      if (newMode != currentMode) {
        currentMode = newMode;
        isAuthenticated = false; // Reset xác thực ngay khi chuyển chế độ
        authSuccessMillis = 0;
        armedAlarmLatched = false; // Reset chốt còi báo động khi chuyển sang bất kỳ chế độ nào
        if (currentMode == MODE_DISARMED) {
          forcedAlarm = false;
        }
        triggerBuzzerPattern(1, 150, 100); // Beep 1 lần phản hồi chuyển chế độ
        statusPublishPending = true; // Gửi trạng thái an toàn ngoài loop()
      }
      xSemaphoreGive(sharedStateMutex);
    }
  }
  // Lệnh bắt đầu hoặc hủy quy trình nạp khuôn mặt
  else if (strcmp(topic, topic_cmd_enroll) == 0) {
    const char* cmd = doc["cmd"] | "";
    if (xSemaphoreTake(sharedStateMutex, (TickType_t)10) == pdTRUE) {
      if (strcmp(cmd, "CANCEL") == 0 || strcmp(cmd, "STOP") == 0 || (doc.containsKey("enable") && !doc["enable"])) {
        isEnrolling = false;
        enrollStep = 0;
        triggerBuzzerPattern(2, 80, 80); // 2 tiếng beep ngắn phản hồi đã hủy nạp mặt
        Serial.println("[AI] 🛑 Đã nhận lệnh HỦY quy trình nạp khuôn mặt!");
      } else {
        isEnrolling = true;
        enrollStep = 1;
        Serial.println("[AI] ▶️ Bắt đầu quy trình nạp khuôn mặt 3 bước!");
      }
      xSemaphoreGive(sharedStateMutex);
    }
  }
  // Lệnh xóa khuôn mặt khỏi Flash MCU
  else if (strcmp(topic, topic_cmd_delete) == 0) {
    int faceId = doc["face_id"];
    bool deleted = deleteFaceFromNVS(faceId);
    deleteDoneFaceId = faceId;
    deleteDoneResult = deleted;
    deleteDoneEventPending = true; // Gửi sự kiện an toàn ngoài loop()
  }
  // Lệnh bật/tắt còi cưỡng bức khẩn cấp hoặc tắt còi trực tiếp từ Web
  else if (strcmp(topic, topic_cmd_alarm) == 0) {
    bool alarmState = doc["alarm"] | false;
    forcedAlarm = alarmState;
    if (!alarmState) {
      // Tắt trực tiếp còi qua Web khi đang hú BUZZER_ON
      armedAlarmLatched = false;
    }
  }
  // Lệnh bật/tắt/điều tiết stream (Auto-Throttling khi viewers.length == 0 hoặc theo yêu cầu từ Web)
  else if (strcmp(topic, topic_cmd_stream) == 0) {
    if (doc.containsKey("enable")) {
      bool en = doc["enable"];
      if (xSemaphoreTake(sharedStateMutex, (TickType_t)10) == pdTRUE) {
        streamEnabled = en;
        xSemaphoreGive(sharedStateMutex);
      }
      Serial.printf("[MQTT] -> Nhận lệnh điều khiển Camera Stream: %s\n", en ? "BẬT (ON)" : "TẮT (OFF)");
    }
  }
  // Lệnh cấu hình thời gian ân hạn mở cửa (5 - 15 giây)
  else if (strcmp(topic, topic_cmd_config) == 0) {
    if (doc.containsKey("grace_period")) {
      int sec = doc["grace_period"];
      if (sec >= 3 && sec <= 60) {
        gracePeriodMs = sec * 1000UL;
      }
    }
  }
}

// Tái kết nối MQTT không chặn (Non-blocking) để đảm bảo hoạt động ngoại tuyến khi mất Wi-Fi
void tryReconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastMqttRetry < 5000) return; // Thử lại sau mỗi 5 giây
  lastMqttRetry = millis();

  StaticJsonDocument<64> lwt;
  lwt["status"] = "OFFLINE";
  char lwtBuf[64];
  serializeJson(lwt, lwtBuf);

  if (mqttClient.connect(DEVICE_ID, mqtt_user, mqtt_pass, topic_status, 1, true, lwtBuf)) {
    publishStatus();
    mqttClient.subscribe(topic_cmd_mode);
    mqttClient.subscribe(topic_cmd_enroll);
    mqttClient.subscribe(topic_cmd_delete);
    mqttClient.subscribe(topic_cmd_alarm);
    mqttClient.subscribe(topic_cmd_stream);
    mqttClient.subscribe(topic_cmd_config);
  }
}

// =================================================================================================
// 9. SETUP & LOOP (CORE 0: NETWORKING, I/O & AN TOÀN NGOẠI TUYẾN)
// =================================================================================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Đợi cổng Serial Monitor ổn định sau khi cắm nạp
  
  Serial.println("\n\n==========================================");
  Serial.println("  ESP32-S3 EDGE AI DOORLOCK SYSTEM BOOT");
  Serial.println("==========================================");

  // 1. Khởi tạo Mutex và Queue đồng bộ Core 0 & Core 1
  sharedStateMutex = xSemaphoreCreateMutex();
  aiEventQueue     = xQueueCreate(10, sizeof(AIEventMsg));

  // 2. Khởi tạo chân I/O ngoại vi
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Chống hú còi lúc khởi động

  pinMode(SW_PIN_1, INPUT_PULLUP);
  pinMode(SW_PIN_2, INPUT_PULLUP);

  pinMode(DOOR_PIN, INPUT_PULLUP);
  isDoorOpen = (digitalRead(DOOR_PIN) == HIGH);
  attachInterrupt(digitalPinToInterrupt(DOOR_PIN), onDoorInterrupt, CHANGE);

  pinMode(LED_R_PIN, OUTPUT); digitalWrite(LED_R_PIN, LOW);
  pinMode(LED_G_PIN, OUTPUT); digitalWrite(LED_G_PIN, LOW);
  pinMode(LED_B_PIN, OUTPUT); digitalWrite(LED_B_PIN, LOW);

  // Khởi tạo tên topic MQTT phân cấp động theo DEVICE_ID
  snprintf(topic_status, sizeof(topic_status), "device/%s/status", DEVICE_ID);
  snprintf(topic_event_door, sizeof(topic_event_door), "device/%s/events/door", DEVICE_ID);
  snprintf(topic_event_alarm, sizeof(topic_event_alarm), "device/%s/events/alarm", DEVICE_ID);
  snprintf(topic_event_enroll_step, sizeof(topic_event_enroll_step), "device/%s/events/enroll_step", DEVICE_ID);
  snprintf(topic_event_enroll_done, sizeof(topic_event_enroll_done), "device/%s/events/enroll_done", DEVICE_ID);
  snprintf(topic_event_deleted_done, sizeof(topic_event_deleted_done), "device/%s/events/deleted_done", DEVICE_ID);
  snprintf(topic_cmd_enroll, sizeof(topic_cmd_enroll), "device/%s/cmd/enroll", DEVICE_ID);
  snprintf(topic_cmd_delete, sizeof(topic_cmd_delete), "device/%s/cmd/delete_face", DEVICE_ID);
  snprintf(topic_cmd_mode, sizeof(topic_cmd_mode), "device/%s/cmd/mode", DEVICE_ID);
  snprintf(topic_cmd_alarm, sizeof(topic_cmd_alarm), "device/%s/cmd/alarm", DEVICE_ID);
  snprintf(topic_cmd_stream, sizeof(topic_cmd_stream), "device/%s/cmd/stream", DEVICE_ID);
  snprintf(topic_cmd_config, sizeof(topic_cmd_config), "device/%s/cmd/config", DEVICE_ID);
  snprintf(breach_upload_url, sizeof(breach_upload_url), "http://%s:%d/api/logs/breach-capture", serverIP, ws_port);

  // Đọc chế độ an ninh khởi tạo từ công tắc vật lý
  currentMode = readSwitchModeWithDebounce();

  // 3. Khởi tạo Camera (100% chuẩn CameraWebServer)
  bool camOk = initCamera();
  if (!camOk) {
    Serial.println("[SYSTEM] ❌ KHỞI TẠO CAMERA THẤT BẠI! Hãy kiểm tra cáp dẹp và chân nối.");
  }

  // 4. Kết nối Wi-Fi (Non-blocking timeout)
  Serial.printf("[NET] Đang kết nối Wi-Fi: %s ...\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false); // Vô hiệu hóa chế độ ngủ Modem Wi-Fi: Giảm độ trễ gói tin
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 5000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[NET] ✅ Đã kết nối Wi-Fi thành công! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[NET] ⚠️ Không kết nối được Wi-Fi trong 5s. Tiếp tục chạy chế độ ngoại tuyến.");
  }

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(2);

  webSocket.begin(ws_host, ws_port, ws_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(3000);

  // 5. Cấu hình OTA an toàn
  ArduinoOTA.setHostname("esp32s3-doorlock");
  ArduinoOTA.onStart([]() {
    if (aiCameraTaskHandle != NULL) {
      vTaskDelete(aiCameraTaskHandle);
    }
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_R_PIN, LOW);
    digitalWrite(LED_G_PIN, LOW);
    digitalWrite(LED_B_PIN, LOW);
    esp_camera_deinit();
  });
  ArduinoOTA.begin();

  // 6. Khởi chạy tác vụ AI & Camera trên Core 1 sau khi toàn bộ hệ thống đã sẵn sàng
  if (camOk) {
    Serial.println("[SYSTEM] ✅ Khởi chạy tác vụ AI Camera trên Core 1...");
    xTaskCreatePinnedToCore(
      aiCameraTask,
      "aiCameraTask",
      12288, // Tăng stack lên 12KB chống tràn stack khi gọi HTTPClient
      NULL,
      1, // Priority 1: Chuẩn hợp tác FreeRTOS
      &aiCameraTaskHandle,
      1  // Core 1
    );
  }
}

void loop() {
  // 1. Luôn ưu tiên đọc công tắc chuyển chế độ vật lý ngay đầu vòng lặp
  SystemMode swMode = readSwitchModeWithDebounce();
  static SystemMode prevSw = currentMode;
  if (swMode != prevSw) {
    prevSw = swMode;
    if (xSemaphoreTake(sharedStateMutex, (TickType_t)10) == pdTRUE) {
      currentMode = swMode;
      isEnrolling = false; // Tự động hủy nạp mặt nếu đang nạp mà gạt công tắc
      enrollStep = 0;
      isAuthenticated = false; // Reset xác thực ngay khi gạt công tắc
      authSuccessMillis = 0;
      armedAlarmLatched = false; // Luôn reset chốt còi báo động khi gạt công tắc đổi chế độ
      if (currentMode == MODE_DISARMED) {
        forcedAlarm = false;
        BuzzerControl(BUZZER_OFF);
      }
      triggerBuzzerPattern(1, 150, 100); // Beep 1 lần khi gạt công tắc chuyển chế độ
      statusPublishPending = true;
      xSemaphoreGive(sharedStateMutex);
    }
  }

  // 2. Tiếp nhận sự kiện từ AI Task (Core 1) gửi sang qua Queue
  AIEventMsg evt;
  if (xQueueReceive(aiEventQueue, &evt, 0) == pdTRUE) {
    switch (evt.type) {
      case AI_EVT_AUTH_SUCCESS:
        // Xác thực khuôn mặt thành công: kích hoạt thời gian ân hạn mở cửa
        isAuthenticated = true;
        authSuccessMillis = millis();
        triggerBuzzerPattern(1, 100, 100); // 1 beep ngắn xác thực
        break;

      case AI_EVT_ENROLL_STEP_OK:
        if (evt.step == 1) {
          triggerBuzzerPattern(1, 150, 150); // 1 tiếng beep góc 1 (Trực diện)
          sendEnrollStepEvent(1, "FRONT");
        } else if (evt.step == 2) {
          triggerBuzzerPattern(2, 120, 120); // 2 tiếng beep góc 2 (Nghiêng 1)
          sendEnrollStepEvent(2, "YAW_1");
        } else if (evt.step == 3) {
          triggerBuzzerPattern(3, 120, 120); // 3 tiếng beep góc 3 (Nghiêng 2)
          sendEnrollStepEvent(3, "YAW_2");
        }
        break;

      case AI_EVT_ENROLL_FINISHED:
        // Còi phát 1 tiếng beep dài (~500ms), kết thúc nạp mặt
        triggerBuzzerPattern(1, 500, 50);
        if (xSemaphoreTake(sharedStateMutex, (TickType_t)10) == pdTRUE) {
          isEnrolling = false;
          enrollStep = 0;
          xSemaphoreGive(sharedStateMutex);
        }
        sendEnrollDoneEvent(evt.face_id);
        break;

      default:
        break;
    }
  }

  // 3. Xử lý sự kiện cảm biến ngắt cửa MC-38
  if (doorStateChanged) {
    doorStateChanged = false;
    sendDoorEvent(isDoorOpen);

    // Nếu cửa đóng lại: reset cờ chụp ảnh và khóa an ninh nếu trước đó mở hợp lệ
    if (!isDoorOpen) {
      breachSnapshotTaken = false; // Cho phép chụp lại khi có lần mở cửa tiếp theo
      if (isAuthenticated) {
        isAuthenticated = false;
        Serial.println("[SYSTEM] 🚪 Cửa đã đóng lại -> Khóa an ninh & kết thúc thời gian ân hạn.");
      }
    }

    // Nếu cửa mở trong chế độ ARMED/STAY mà chưa xác thực: phát cảnh báo đột nhập
    if (isDoorOpen && !isAuthenticated && currentMode != MODE_DISARMED) {
      sendAlarmBreachEvent();
      if (currentMode == MODE_ARMED) {
        armedAlarmLatched = true; // Kích hoạt chốt còi hú công suất tối đa
        if (!breachSnapshotTaken) {
          breachSnapshotTaken = true; // Chỉ kích hoạt chụp đúng 1 lần cho mỗi lần cửa mở
          captureBreachRequested = true;
        }
      }
    }
  }

  // 4. Quản lý bộ đếm thời gian ân hạn mở cửa (Grace Period Timeout)
  if (isAuthenticated && (millis() - authSuccessMillis >= gracePeriodMs)) {
    isAuthenticated = false;
  }

  // 5. Điều khiển chỉ thị LED RGB & Còi cảnh báo (Chạy mượt mà, độc lập với mạng)
  if (isEnrolling) {
    // Chế độ đăng ký mặt: LED nhấp nháy màu Vàng (Đỏ + Xanh lá)
    setRGB(true, true, false, MODE_BLINK);
    BuzzerControl(BUZZER_OFF);
  }
  else if (currentMode == MODE_DISARMED) {
    // Chế độ DISARMED: LED Xanh dương (Blue), Tắt còi, cho phép mở cửa tự do
    armedAlarmLatched = false;
    BuzzerControl(BUZZER_OFF);
    if (isDoorOpen) {
      setRGB(false, true, false, MODE_BLINK); // Cửa mở tự do: nhấp nháy Xanh lá
    } else {
      setRGB(false, true, false, MODE_STEADY);// Cửa đóng: Xanh lá sáng đứng
    }
  }
  else if (currentMode == MODE_ARMED) {
    if (forcedAlarm || armedAlarmLatched) {
      // Còi khẩn cấp hoặc Đột nhập: Còi hú tối đa + Đỏ chớp nhanh
      BuzzerControl(BUZZER_ON);
      setRGB(true, false, false, MODE_FAST_BLINK);
    }
    else if (isAuthenticated) {
      // Xác thực khuôn mặt thành công trong ARMED: LED Xanh lá sáng đứng
      BuzzerControl(BUZZER_OFF);
      setRGB(false, true, false, MODE_STEADY);
    }
    else if (isDoorOpen) {
      // Cửa mở trái phép: Kích hoạt còi hú
      armedAlarmLatched = true;
      BuzzerControl(BUZZER_ON);
      setRGB(true, false, false, MODE_FAST_BLINK);
    }
    else {
      // Cửa đóng an toàn trong ARMED: LED Đỏ sáng đứng
      BuzzerControl(BUZZER_OFF);
      setRGB(true, false, false, MODE_STEADY);
    }
  }
  else if (currentMode == MODE_STAY) {
    if (forcedAlarm) {
      // Còi khẩn cấp Panic: Còi hú tối đa + Đỏ chớp nhanh
      BuzzerControl(BUZZER_ON);
      setRGB(true, false, false, MODE_FAST_BLINK);
    }
    else if (isAuthenticated) {
      // Xác thực khuôn mặt thành công trong STAY: LED Xanh lá sáng đứng
      BuzzerControl(BUZZER_OFF);
      setRGB(false, true, false, MODE_STEADY);
    }
    else if (isDoorOpen) {
      // Cửa mở trái phép trong STAY: Còi beep định kỳ + Vàng nhấp nháy
      BuzzerControl(BUZZER_BEEP);
      setRGB(true, true, false, MODE_BLINK);
    }
    else {
      // Cửa đóng an toàn trong STAY: LED Vàng sáng đứng
      BuzzerControl(BUZZER_OFF);
      setRGB(true, true, false, MODE_STEADY);
    }
  }

  // 6. Xử lý Mạng, MQTT & OTA (Non-blocking, không cản trở ngoại vi)
  static unsigned long lastWifiRetry = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiRetry >= 10000) {
      lastWifiRetry = millis();
      WiFi.reconnect();
    }
  }

  // Xử lý nạp OTA
  ArduinoOTA.handle();

  // Quản lý kết nối MQTT (Core 0)
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      tryReconnectMQTT();
    } else {
      mqttClient.loop();
    }

    // Gửi phản hồi MQTT an toàn ngoài loop() (Tránh deadlock/corrupt buffer trong callback)
    if (statusPublishPending && mqttClient.connected()) {
      statusPublishPending = false;
      publishStatus();
    }
    if (deleteDoneEventPending && mqttClient.connected()) {
      deleteDoneEventPending = false;
      sendDeletedDoneEvent(deleteDoneFaceId, deleteDoneResult);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(2)); // Nhường nhẹ CPU
}