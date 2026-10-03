#include <Arduino.h>
#include "env.h"
#include <atomic>
#include <esp_arduino_version.h>
#if ESP_ARDUINO_VERSION_MAJOR != 2 || ESP_ARDUINO_VERSION_MINOR != 0 || ESP_ARDUINO_VERSION_PATCH != 17
#error "Select esp32 by Espressif Systems 2.0.17 in Boards Manager"
#endif
#if !CONFIG_IDF_TARGET_ESP32S3
#error "Select ESP32S3 Dev Module"
#endif
#include <WiFi.h>
#include <WiFiUdp.h>
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

#if ENABLE_BLE_FALLBACK
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#endif

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
char        serverIP[16]  = SERVER_IP;

const char* ssid          = WIFI_SSID;
const char* password      = WIFI_PASS;

const int   mqtt_port     = MQTT_PORT;
const char* mqtt_user     = "esp32_client";
const char* mqtt_pass     = "esp32_pass_secure";

int         ws_port       = WS_PORT;
const char* ws_path       = "/ws/camera/stream";

const char* DEVICE_ID     = "dev_01";
char topic_status[64];
char topic_event_door[64];
char topic_event_alarm[64];
char topic_event_auth[64];
char topic_event_enroll_step[64];
char topic_event_enroll_done[64];
char topic_event_deleted_done[64];
char topic_cmd_enroll[64];
char topic_cmd_delete[64];
char topic_cmd_mode[64];
char topic_cmd_alarm[64];
char topic_cmd_stream[64];
char topic_cmd_config[64];
char topic_cmd_toggle_face[64];
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
  AI_EVT_ENROLL_FAILED,
  AI_EVT_DELETE_DONE
};

struct AIEventMsg {
  AIEventType type;
  int face_id;
  int step;
  uint32_t epoch;
  uint32_t frameMillis;
  char reason[48];
};
static std::atomic<uint32_t> authEpoch{1};
static std::atomic<uint32_t> authCooldownMillis{0};


// Queue & Mutex cho FreeRTOS
QueueHandle_t     aiEventQueue      = NULL;
SemaphoreHandle_t sharedStateMutex  = NULL;

// Trạng thái an ninh chia sẻ giữa 2 core (bảo vệ bằng Mutex)
std::atomic<SystemMode> currentMode{MODE_DISARMED};
std::atomic<bool> isDoorOpen{false};
std::atomic<bool> doorStateChanged{false};
std::atomic<bool> isAuthenticated{false};
std::atomic<bool> isEnrolling{false};
std::atomic<int> enrollStep{0};
std::atomic<bool> streamEnabled{false}; // Mặc định TẮT stream, chỉ stream khi có yêu cầu từ Web
std::atomic<bool> forcedAlarm{false};
std::atomic<bool> armedAlarmLatched{false}; // Chốt còi hú công suất tối đa khi bị đột nhập ở MODE_ARMED
std::atomic<bool> captureBreachRequested{false}; // Yêu cầu Core 1 chụp ảnh bằng chứng vi phạm chất lượng cao
std::atomic<bool> breachSnapshotTaken{false}; // Cờ chốt chống chụp lặp khi cửa đang mở

unsigned long       authSuccessMillis = 0;
std::atomic<unsigned long> gracePeriodMs{10000}; // Mặc định 10 giây (tùy chỉnh 5 - 15 giây từ xa qua Web)

// Quản lý Flash NVS
const char* PREF_NAMESPACE = "face_nvs";

// Client mạng
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebSocketsClient webSocket;
TaskHandle_t aiCameraTaskHandle = NULL;

// Non-blocking MQTT reconnect timer & Cờ gửi tin an toàn
unsigned long lastMqttRetry = 0;
std::atomic<bool> statusPublishPending{false};
std::atomic<bool> deleteDoneEventPending{false};
std::atomic<int> deleteDoneFaceId{0};
std::atomic<bool> deleteDoneResult{false};

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
#if ALWAYS_DOOR_CLOSE_DEMO
    isDoorOpen = false; // Demo: luôn coi cửa đóng
#else
    isDoorOpen = (digitalRead(DOOR_PIN) == HIGH); // Hở mạch = Cửa mở
#endif
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

  if ((millis() - lastDebounceTime) >= 30) {
    stableMode = rawMode;
  }

  return stableMode;
}

// Hàm chuyển chế độ an ninh dùng chung cho cả MQTT và BLE Fallback
bool applySystemModeChange(SystemMode newMode, const char* source) {
  if (xSemaphoreTake(sharedStateMutex, (TickType_t)20) == pdTRUE) {
    if (newMode != currentMode) {
      ++authEpoch;
      isEnrolling = false; enrollStep = 0;
      currentMode = newMode;
      isAuthenticated = false; // Reset xác thực ngay khi chuyển chế độ
      authSuccessMillis = 0;
      armedAlarmLatched = false; // Reset chốt còi báo động
      if (currentMode == MODE_DISARMED) {
        forcedAlarm = false;
      }
      triggerBuzzerPattern(1, 150, 100); // Beep 1 lần phản hồi chuyển chế độ
      statusPublishPending = true; // Gửi trạng thái qua MQTT nếu có kết nối
      const char* modeStr = (newMode == MODE_ARMED) ? "ARMED" : (newMode == MODE_STAY) ? "STAY" : "DISARMED";
      Serial.printf("[MODE] 🔄 Đã đổi chế độ an ninh sang: %s (Nguồn: %s)\n", modeStr, source);
    }
    xSemaphoreGive(sharedStateMutex);
    return true;
  }
  return false;
}

#if ENABLE_BLE_FALLBACK
// =================================================================================================
// CẤU HÌNH & XỬ LÝ BLUETOOTH DỰ PHÒNG (BLE iBeacon Scanner)
// =================================================================================================
static bool bleActive = false;
static unsigned long lastBleScanRestart = 0;

static uint8_t blePatternDisarmed[32];
static size_t  bleLenDisarmed = 0;
static uint8_t blePatternArmed[32];
static size_t  bleLenArmed = 0;
static uint8_t blePatternStay[32];
static size_t  bleLenStay = 0;
static bool    blePatternsInit = false;

static size_t hexToBytes(const char* hex, uint8_t* out, size_t maxLen) {
  size_t hexLen = strlen(hex);
  size_t bytes = 0;
  for (size_t i = 0; i + 1 < hexLen && bytes < maxLen; i += 2) {
    char byteStr[3] = { hex[i], hex[i + 1], '\0' };
    out[bytes++] = (uint8_t)strtoul(byteStr, NULL, 16);
  }
  return bytes;
}

static void initBlePatterns() {
  if (blePatternsInit) return;
  bleLenDisarmed = hexToBytes(DISARMED_BLE_HEX, blePatternDisarmed, sizeof(blePatternDisarmed));
  bleLenArmed    = hexToBytes(ARMED_BLE_HEX, blePatternArmed, sizeof(blePatternArmed));
  bleLenStay     = hexToBytes(STAY_BLE_HEX, blePatternStay, sizeof(blePatternStay));
  blePatternsInit = true;
}

static esp_ble_scan_params_t ble_scan_params = {
  .scan_type              = BLE_SCAN_TYPE_ACTIVE,
  .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
  .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
  .scan_interval          = 0x50, // 50ms
  .scan_window            = 0x30, // 30ms (tiết kiệm CPU)
  .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
    esp_ble_gap_start_scanning(30);
  } else if (event == ESP_GAP_BLE_SCAN_RESULT_EVT) {
    esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
    if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
      uint8_t *adv_data = scan_result->scan_rst.ble_adv;
      uint8_t adv_data_len = scan_result->scan_rst.adv_data_len;

      initBlePatterns();

      static unsigned long lastBleCmdMillis = 0;
      if (millis() - lastBleCmdMillis < 1500) return; // Chống lặp lệnh trong 1.5s

      for (uint8_t i = 0; i < adv_data_len; i++) {
        // So khớp với mẫu DISARMED_BLE_HEX (so sánh 22 byte cấu trúc: prefix + UUID + Major + Minor)
        size_t lenDis = (bleLenDisarmed >= 22) ? 22 : bleLenDisarmed;
        if (lenDis > 0 && i + lenDis <= adv_data_len && memcmp(adv_data + i, blePatternDisarmed, lenDis) == 0) {
          lastBleCmdMillis = millis();
          if (BLE_DEBUG) {
            Serial.printf("[BLE] 📡 Nhận gói tin khớp DISARMED_BLE_HEX (RSSI: %d dBm)\n", scan_result->scan_rst.rssi);
          }
          applySystemModeChange(MODE_DISARMED, "BLE iBeacon");
          break;
        }

        // So khớp với mẫu ARMED_BLE_HEX
        size_t lenArm = (bleLenArmed >= 22) ? 22 : bleLenArmed;
        if (lenArm > 0 && i + lenArm <= adv_data_len && memcmp(adv_data + i, blePatternArmed, lenArm) == 0) {
          lastBleCmdMillis = millis();
          if (BLE_DEBUG) {
            Serial.printf("[BLE] 📡 Nhận gói tin khớp ARMED_BLE_HEX (RSSI: %d dBm)\n", scan_result->scan_rst.rssi);
          }
          applySystemModeChange(MODE_ARMED, "BLE iBeacon");
          break;
        }

        // So khớp với mẫu STAY_BLE_HEX
        size_t lenStay = (bleLenStay >= 22) ? 22 : bleLenStay;
        if (lenStay > 0 && i + lenStay <= adv_data_len && memcmp(adv_data + i, blePatternStay, lenStay) == 0) {
          lastBleCmdMillis = millis();
          if (BLE_DEBUG) {
            Serial.printf("[BLE] 📡 Nhận gói tin khớp STAY_BLE_HEX (RSSI: %d dBm)\n", scan_result->scan_rst.rssi);
          }
          applySystemModeChange(MODE_STAY, "BLE iBeacon");
          break;
        }
      }
    }
  }
}

void handleBleFallback(bool enableLocalBle) {
  if (enableLocalBle && !bleActive) {
    initBlePatterns();
    // Chỉ bật BLE nếu còn đủ bộ nhớ RAM cho Bluedroid (~60KB)
    if (ESP.getFreeHeap() < 65000) {
      if (BLE_DEBUG) {
        Serial.printf("[BLE] ⚠️ Heap quá thấp (%d bytes), hoãn bật BLE dự phòng để tránh tràn RAM!\n", ESP.getFreeHeap());
      }
      return;
    }

    if (BLE_DEBUG) {
      Serial.printf("[BLE] 🚀 Bật kênh BLE iBeacon dự phòng (Free Heap: %d bytes)...\n", ESP.getFreeHeap());
      Serial.printf("[BLE] -> Mẫu DISARMED: %s\n", DISARMED_BLE_HEX);
      Serial.printf("[BLE] -> Mẫu ARMED:    %s\n", ARMED_BLE_HEX);
      Serial.printf("[BLE] -> Mẫu STAY:     %s\n", STAY_BLE_HEX);
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) == ESP_OK) {
      if (esp_bt_controller_enable(ESP_BT_MODE_BLE) == ESP_OK) {
        if (esp_bluedroid_init() == ESP_OK && esp_bluedroid_enable() == ESP_OK) {
          esp_ble_gap_register_callback(esp_gap_cb);
          esp_ble_gap_set_scan_params(&ble_scan_params);
          bleActive = true;
          lastBleScanRestart = millis();
          if (BLE_DEBUG) {
            Serial.printf("[BLE] ✅ BLE Scanner đã sẵn sàng nhận lệnh! (Free Heap: %d bytes)\n", ESP.getFreeHeap());
          }
          return;
        }
      }
    }
    // Nếu khởi tạo lỗi, giải phóng
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    bleActive = false;
  }
  else if (!enableLocalBle && bleActive) {
    if (BLE_DEBUG) {
      Serial.println("[BLE] 🛑 Tắt BLE dự phòng, ưu tiên Wi-Fi/Server & hoàn trả 100% RAM...");
    }
    esp_ble_gap_stop_scanning();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    bleActive = false;
    if (BLE_DEBUG) {
      Serial.printf("[BLE] ✅ Đã đóng BLE hoàn toàn. Free Heap phục hồi: %d bytes\n", ESP.getFreeHeap());
    }
  }
  else if (bleActive) {
    // Định kỳ khởi động lại lượt quét 30s để duy trì lắng nghe liên tục
    if (millis() - lastBleScanRestart >= 30000) {
      lastBleScanRestart = millis();
      esp_ble_gap_start_scanning(30);
    }
  }
}
#endif


// =================================================================================================
// 5. CẤU HÌNH CAMERA OV5640 (QVGA 320x240 - SINGLE-CHANNEL GRAYSCALE JPEG)
// =================================================================================================
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      if (WS_DEBUG) {
        Serial.printf("⚠️ [WS CAMERA] Ngắt kết nối tới máy chủ stream (IP: %s:%d)! WiFi RSSI: %d dBm\n",
                      serverIP, ws_port, WiFi.RSSI());
      }
      break;
    case WStype_CONNECTED:
      if (WS_DEBUG) {
        Serial.printf("✅ [WS CAMERA] Đã kết nối WebSocket thành công: %s (Server: %s:%d, RSSI: %d dBm)\n",
                      (char*)payload, serverIP, ws_port, WiFi.RSSI());
      }
      break;
    case WStype_TEXT:
      if (WS_DEBUG) {
        Serial.printf("[WS CAMERA] Nhận phản hồi: %.*s\n", (int)length, (const char*)payload);
      }
      break;
    case WStype_BIN:
      break;
    case WStype_ERROR:
      if (WS_DEBUG) {
        Serial.printf("❌ [WS CAMERA] Lỗi giao thức WebSocket! Length=%d. Kiểm tra serverIP=%s:%d, WiFi RSSI=%d dBm\n",
                      (int)length, serverIP, ws_port, WiFi.RSSI());
      }
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

  camera_config_t config = {};
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
  config.frame_size   = FRAMESIZE_QVGA; // Khởi tạo trực tiếp QVGA để DMA tối ưu tốc độ và không lãng phí RAM
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY; // Lấy frame sẵn sàng từ DMA, tránh miss ngắt và tránh timeout 5s
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 18; // Frame ~4KB gọn gàng trong TCP buffer, stream 20fps mượt không nghẽn
  config.fb_count     = 2;  // Double buffer: DMA nạp và Task đọc song song

  // Cấu hình PSRAM chuẩn
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      Serial.printf("-> PSRAM OK: Total %u B, Free %u B\n", (unsigned int)ESP.getPsramSize(), (unsigned int)ESP.getFreePsram());
    } else {
      config.fb_location  = CAMERA_FB_IN_DRAM;
      config.fb_count     = 1;
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

    // Bù sáng ngược sáng (Backlight / Ceiling lamp compensation) & Tự động tăng sáng trong phòng tối
    s->set_exposure_ctrl(s, 1);     // Bật Auto Exposure (AEC)
    s->set_aec2(s, 1);              // Bật thuật toán đo sáng nâng cao DSP AEC2 (ưu tiên trung tâm khuôn mặt thay vì bị lóa bởi đèn trần)
    s->set_ae_level(s, 1);          // Nâng mức phơi sáng +1 EV giúp khuôn mặt bị khuất bóng sáng rõ nét
    s->set_gain_ctrl(s, 1);         // Bật Auto Gain (AGC)
    s->set_gainceiling(s, (gainceiling_t)GAINCEILING_16X); // Tự động tăng sáng nhạy khi phòng tối, tự hạ về 1X khi đủ sáng
  }

  Serial.println("✅ Camera OV5640 khởi tạo thành công (100% chuẩn CameraWebServer)!\n");
  return true;
}

#include "FaceRuntime.h"

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

void sendAuthEvent(int faceId) {
  StaticJsonDocument<128> doc;
  doc["face_id"] = faceId;
  doc["status"]  = "SUCCESS";
  char buf[128];
  serializeJson(doc, buf);
  mqttClient.publish(topic_event_auth, buf);
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
    const char* mode = doc["mode"] | "";
    SystemMode newMode = currentMode.load();
    if (strcmp(mode, "ARMED") == 0) {
      newMode = MODE_ARMED;
    } else if (strcmp(mode, "STAY") == 0) {
      newMode = MODE_STAY;
    } else if (strcmp(mode, "DISARMED") == 0) {
      newMode = MODE_DISARMED;
    }
    applySystemModeChange(newMode, "MQTT");
  }
  // Lệnh bắt đầu hoặc hủy quy trình nạp khuôn mặt
  else if (strcmp(topic, topic_cmd_enroll) == 0) {
    const char* cmd = doc["cmd"] | "";
    if (xSemaphoreTake(sharedStateMutex, (TickType_t)10) == pdTRUE) {
      ++authEpoch;
      isAuthenticated = false; authSuccessMillis = 0;
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
  // Lệnh xóa khuôn mặt khỏi Flash MCU (xóa 1 mặt hoặc xóa toàn bộ)
  else if (strcmp(topic, topic_cmd_delete) == 0) {
    int id = ((doc["all"] | false) || (doc["face_id"] | 0) == -1) ? -1 : (doc["face_id"] | 0);
    xSemaphoreTake(sharedStateMutex, portMAX_DELAY);
    ++authEpoch; isAuthenticated = false; isEnrolling = false; enrollStep = 0;
    xSemaphoreGive(sharedStateMutex);
    bool queued=false;
    if (id != 0 && deleteQueue && !deletePending.exchange(true)) {
      queued=xQueueSend(deleteQueue,&id,0)==pdTRUE;
      if(!queued) deletePending=false;
    }
    if(!queued) {
      StaticJsonDocument<128> reply;
      reply["face_id"]=id; reply["status"]="FAILED"; reply["reason"]="BUSY_OR_INVALID_ID";
      char buf[128];serializeJson(reply,buf);mqttClient.publish(topic_event_deleted_done,buf);
    }
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
  // Lệnh bật/tắt kích hoạt khuôn mặt (Active / Inactive) từ Web
  else if (strcmp(topic, topic_cmd_toggle_face) == 0) {
    int faceId = doc["face_id"] | 0;
    bool active = doc.containsKey("active") ? (bool)doc["active"] : true;
    if (faceId > 0 && faceId <= 31) {
      FaceEngine::setFaceActive(faceId, active);
      Serial.printf("[FACE] Cập nhật person=%d active=%s (mask=0x%08X)\n",
                    faceId, active ? "BẬT" : "TẮT", FaceEngine::activeFacesMask.load());
    }
  }
}

// Tái kết nối MQTT không chặn (Non-blocking) để đảm bảo hoạt động ngoại tuyến khi mất Wi-Fi
void tryReconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED || strlen(serverIP) == 0) return;
  if (millis() - lastMqttRetry < 5000) return; // Thu lai sau moi 5 giay
  lastMqttRetry = millis();

  StaticJsonDocument<64> lwt;
  lwt["status"] = "OFFLINE";
  char lwtBuf[64];
  serializeJson(lwt, lwtBuf);

  Serial.printf("[MQTT] 🔄 Đang thử kết nối broker %s:%d (User: %s)...\n", serverIP, mqtt_port, mqtt_user);
  if (mqttClient.connect(DEVICE_ID, mqtt_user, mqtt_pass, topic_status, 1, true, lwtBuf)) {
    Serial.printf("✅ [MQTT] Đã kết nối Broker thành công! ClientID=%s (WiFi RSSI: %d dBm)\n", DEVICE_ID, WiFi.RSSI());
    publishStatus();
    mqttClient.subscribe(topic_cmd_mode);
    mqttClient.subscribe(topic_cmd_enroll);
    mqttClient.subscribe(topic_cmd_delete);
    mqttClient.subscribe(topic_cmd_alarm);
    mqttClient.subscribe(topic_cmd_stream);
    mqttClient.subscribe(topic_cmd_config);
    mqttClient.subscribe(topic_cmd_toggle_face);
  } else {
    // Mã lỗi: -4: TIMEOUT, -3: LOST_CONN, -2: CONNECT_FAILED, 1: BAD_PROTO, 2: BAD_ID, 4: BAD_CREDENTIALS, 5: UNAUTHORIZED
    Serial.printf("❌ [MQTT] Kết nối thất bại, state = %d. WiFi RSSI: %d dBm (Kiểm tra IP/Port/User/Pass/Broker)\n",
                  mqttClient.state(), WiFi.RSSI());
  }
}

// ponytail: UDP discovery with auto-retry in loop, both active ping and passive listen
bool discoverServerIP(uint32_t timeoutMs = 4000) {
  if (strlen(serverIP) > 0) return true;
  WiFiUDP udp;
  if (!udp.begin(8888)) {
    Serial.println("[NET] ❌ Không thể mở UDP port 8888!");
    return false;
  }
  Serial.printf("[NET] 🔍 Đang tìm kiếm Server qua UDP port 8888 (chờ %ds)...\n", timeoutMs / 1000);

  // Gửi gói tin chủ động hỏi Server (cả 255.255.255.255 và Directed Subnet Broadcast)
  udp.beginPacket("255.255.255.255", 8888);
  udp.print("{\"cmd\":\"DISCOVER_SERVER\"}");
  udp.endPacket();

  IPAddress bcast = ~WiFi.subnetMask() | WiFi.localIP();
  udp.beginPacket(bcast, 8888);
  udp.print("{\"cmd\":\"DISCOVER_SERVER\"}");
  udp.endPacket();

  char buf[128];
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (udp.parsePacket() > 0) {
      int n = udp.read(buf, sizeof(buf) - 1);
      buf[n > 0 ? n : 0] = '\0';
      if (strstr(buf, "\"service\":\"esp32_security_backend\"")) {
        snprintf(serverIP, sizeof(serverIP), "%s", udp.remoteIP().toString().c_str());
        char* p = strstr(buf, "\"port\":");
        if (p) sscanf(p + 7, "%d", &ws_port);
        Serial.printf("[NET] 🎯 Discovered server: %s:%d\n", serverIP, ws_port);
        udp.stop();
        return true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  udp.stop(); // ponytail: drop socket immediately to free lwIP PCB
  return false;
}

static bool networkClientsConfigured=false;
void initNetworkClients() {
  if (strlen(serverIP) == 0) return;
  snprintf(breach_upload_url, sizeof(breach_upload_url), "http://%s:%d/api/logs/breach-capture", serverIP, ws_port);

  mqttClient.setServer(serverIP, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(1);

  wsInitRequested = true; // Only cameraStream task owns WebSocket client
  networkClientsConfigured=true;
  Serial.printf("[NET] 🌐 Khởi tạo kết nối tới Server: %s (WS:%d, MQTT:%d)\n", serverIP, ws_port, mqtt_port);
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

  if (!sharedStateMutex || !aiEventQueue) { Serial.println("[FATAL] RTOS allocation failed"); while(true) delay(1000); }

  // 2. Khởi tạo chân I/O ngoại vi
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Chống hú còi lúc khởi động

  pinMode(SW_PIN_1, INPUT_PULLUP);
  pinMode(SW_PIN_2, INPUT_PULLUP);

  pinMode(DOOR_PIN, INPUT_PULLUP);
#if ALWAYS_DOOR_CLOSE_DEMO
  isDoorOpen = false; // Demo: luôn coi cửa đóng
#else
  isDoorOpen = (digitalRead(DOOR_PIN) == HIGH);
#endif
  attachInterrupt(digitalPinToInterrupt(DOOR_PIN), onDoorInterrupt, CHANGE);

  pinMode(LED_R_PIN, OUTPUT); digitalWrite(LED_R_PIN, LOW);
  pinMode(LED_G_PIN, OUTPUT); digitalWrite(LED_G_PIN, LOW);
  pinMode(LED_B_PIN, OUTPUT); digitalWrite(LED_B_PIN, LOW);

  // Khởi tạo tên topic MQTT phân cấp động theo DEVICE_ID
  snprintf(topic_status, sizeof(topic_status), "device/%s/status", DEVICE_ID);
  snprintf(topic_event_door, sizeof(topic_event_door), "device/%s/events/door", DEVICE_ID);
  snprintf(topic_event_alarm, sizeof(topic_event_alarm), "device/%s/events/alarm", DEVICE_ID);
  snprintf(topic_event_auth, sizeof(topic_event_auth), "device/%s/events/auth", DEVICE_ID);
  snprintf(topic_event_enroll_step, sizeof(topic_event_enroll_step), "device/%s/events/enroll_step", DEVICE_ID);
  snprintf(topic_event_enroll_done, sizeof(topic_event_enroll_done), "device/%s/events/enroll_done", DEVICE_ID);
  snprintf(topic_event_deleted_done, sizeof(topic_event_deleted_done), "device/%s/events/deleted_done", DEVICE_ID);
  snprintf(topic_cmd_enroll, sizeof(topic_cmd_enroll), "device/%s/cmd/enroll", DEVICE_ID);
  snprintf(topic_cmd_delete, sizeof(topic_cmd_delete), "device/%s/cmd/delete_face", DEVICE_ID);
  snprintf(topic_cmd_mode, sizeof(topic_cmd_mode), "device/%s/cmd/mode", DEVICE_ID);
  snprintf(topic_cmd_alarm, sizeof(topic_cmd_alarm), "device/%s/cmd/alarm", DEVICE_ID);
  snprintf(topic_cmd_stream, sizeof(topic_cmd_stream), "device/%s/cmd/stream", DEVICE_ID);
  snprintf(topic_cmd_config, sizeof(topic_cmd_config), "device/%s/cmd/config", DEVICE_ID);
  snprintf(topic_cmd_toggle_face, sizeof(topic_cmd_toggle_face), "device/%s/cmd/toggle_face", DEVICE_ID);
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
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 25000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setSleep(false); // Vô hiệu hóa triệt để Modem Sleep SAU KHI kết nối Wi-Fi!
    Serial.printf("[NET] ✅ Đã kết nối Wi-Fi thành công! IP: %s, Subnet: %s, Gateway: %s, DNS: %s, RSSI: %d dBm, MAC: %s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.subnetMask().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  WiFi.dnsIP().toString().c_str(),
                  WiFi.RSSI(),
                  WiFi.macAddress().c_str());
    if (discoverServerIP(6000)) {
      initNetworkClients();
    } else if (strlen(serverIP) > 0) {
      Serial.printf("[NET] ⚠️ Dùng IP fallback: %s\n", serverIP);
      initNetworkClients();
    } else {
      Serial.println("[NET] ⏳ Chưa tìm thấy Server, sẽ tự động tìm kiếm ngầm trong loop()...");
    }
  } else {
    Serial.println("[NET] ⚠️ Không kết nối được Wi-Fi trong thời gian chờ. Tiếp tục chạy chế độ ngoại tuyến.");
  }

  // 5. Cấu hình OTA an toàn
  ArduinoOTA.setHostname("esp32s3-doorlock");
  ArduinoOTA.onStart([]() {
    otaStopping = true;
    ++authEpoch; isAuthenticated = false; isEnrolling = false;
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_R_PIN, LOW); digitalWrite(LED_G_PIN, LOW); digitalWrite(LED_B_PIN, LOW);
    // Do not delete a task while it holds a frame/mutex or is inside ESP-DL.
    // Workers park cooperatively; OTA completion reboots the device.
  });
  ArduinoOTA.onError([](ota_error_t) {
    Serial.println("[OTA] Failed; restarting to restore a clean camera/AI state");
    ESP.restart();
  });
  ArduinoOTA.begin();

  if (camOk && !startFaceTasks()) {
    Serial.println("[SYSTEM] Cannot start face workers. Authentication DISABLED.");
    aiReady=false;
  }

}

void loop() {
  if (otaStopping) { ArduinoOTA.handle(); delay(2); return; }
  // 1. Luon uu tien doc cong tac chuyen che do vat ly ngay dau vong lap
  SystemMode swMode = readSwitchModeWithDebounce();
  static SystemMode prevSw = currentMode.load();
  if (swMode != prevSw) {
    if (xSemaphoreTake(sharedStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      prevSw = swMode;
      currentMode = swMode;
      ++authEpoch;
      isEnrolling = false; // Tu dong huy nap mat neu dang nap ma gat cong tac
      enrollStep = 0;
      isAuthenticated = false; // Reset xac thuc ngay khi gat cong tac
      authSuccessMillis = 0;
      armedAlarmLatched = false; // Luon reset chot coi bao dong khi gat cong tac doi che do
      if (currentMode == MODE_DISARMED) {
        forcedAlarm = false;
        BuzzerControl(BUZZER_OFF);
      }
      triggerBuzzerPattern(1, 150, 100); // Beep 1 lan khi gat cong tac chuyen che do
      statusPublishPending = true;
      xSemaphoreGive(sharedStateMutex);
    }
  }

  // Consume only fresh events from the current command generation.
  AIEventMsg evt;
  while (xQueueReceive(aiEventQueue, &evt, 0) == pdTRUE) {
    if(evt.type == AI_EVT_DELETE_DONE) {
      sendDeletedDoneEvent(evt.face_id,evt.step==1);
      continue;
    }
    // A committed enrollment remains real even if a later command changed epoch.
    // Report its ID, without cancelling any newer enrollment session.
    if(evt.type == AI_EVT_ENROLL_FINISHED && evt.epoch != authEpoch.load()) {
      sendEnrollDoneEvent(evt.face_id);
      continue;
    }
    if(evt.epoch != authEpoch.load()) continue;
    switch(evt.type) {
      case AI_EVT_AUTH_SUCCESS:
        if(evt.face_id>0 && aiReady && !deletePending && !isEnrolling && !isAuthenticated &&
           currentMode!=MODE_DISARMED && uint32_t(millis()-evt.frameMillis)<=FacePolicy::FRAME_MAX_AGE_MS) {
          ++authEpoch; // Tăng epoch ngay để vô hiệu hóa mọi frame cũ còn tồn đọng trong queue
          isAuthenticated=true; authSuccessMillis=millis();
          triggerBuzzerPattern(1,100,100);
          Serial.printf("[AUTH] VERIFIED person=%d\n",evt.face_id);
          statusPublishPending=true;
          sendAuthEvent(evt.face_id);
        }
        break;
      case AI_EVT_ENROLL_STEP_OK:
        {
          enrollStep=evt.step;
          triggerBuzzerPattern(evt.step,120,120);
          sendEnrollStepEvent(evt.step,evt.step==1?"FRONT":(evt.step==2?"YAW_1":"YAW_2"));
        }
        break;
      case AI_EVT_ENROLL_FINISHED:
        isEnrolling=false;enrollStep=0;
        triggerBuzzerPattern(1,500,50);sendEnrollDoneEvent(evt.face_id);
        break;
      case AI_EVT_ENROLL_FAILED: {
        isEnrolling=false;enrollStep=0;triggerBuzzerPattern(3,80,80);
        StaticJsonDocument<160> reply;
        reply["status"]="FAILED";reply["reason"]=evt.reason;reply["face_id"]=evt.face_id;
        char buf[160];serializeJson(reply,buf);mqttClient.publish(topic_event_enroll_done,buf);
        Serial.printf("[ENROLL] FAILED: %s\n",evt.reason);
        break;
      }
      default: break;
    }
  }

  // 3. Xử lý sự kiện cảm biến ngắt cửa MC-38
  if (doorStateChanged) {
    doorStateChanged = false;
    sendDoorEvent(isDoorOpen);

    // Nếu cửa đóng lại: reset cờ chụp ảnh và khóa an ninh nếu trước đó mở hợp lệ
    if (!isDoorOpen) {
      ++authEpoch;
      breachSnapshotTaken = false; // Cho phép chụp lại khi có lần mở cửa tiếp theo
      if (isAuthenticated) {
        isAuthenticated = false;
        authCooldownMillis = millis() + 2500;
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
    ++authEpoch;
    isAuthenticated = false;
    authCooldownMillis = millis() + 2500;
    triggerBuzzerPattern(1, 150, 100); // Beep 1 tiếng thông báo hết thời gian mở cửa / khóa lại an ninh
    Serial.println("[SYSTEM] ⏱️ Hết thời gian ân hạn (Timeout) -> Khóa lại an ninh & reset phiên nhận diện.");
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
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }

  // Xử lý nạp OTA
  ArduinoOTA.handle();

  // Quản lý kết nối Mạng & Tự động tìm kiếm Server ngầm nếu chưa có
  if (WiFi.status() == WL_CONNECTED) {
    if (strlen(serverIP) == 0) {
      static unsigned long lastDiscRetry = 0;
      if (millis() - lastDiscRetry >= 4000) {
        lastDiscRetry = millis();
        if (discoverServerIP(3000)) {
          initNetworkClients();
        }
      }
    } else {
      if(!networkClientsConfigured) initNetworkClients();
      if (!mqttClient.connected()) {
        tryReconnectMQTT();
      } else {
        mqttClient.loop();
      }
    }

    // Gửi phản hồi MQTT an toàn ngoài loop() (Tránh deadlock/corrupt buffer trong callback)
    if (statusPublishPending && mqttClient.connected()) {
      statusPublishPending = false;
      publishStatus();
    }
    if (deleteDoneEventPending && mqttClient.connected()) {
      deleteDoneEventPending = false;
      sendDeletedDoneEvent(deleteDoneFaceId.load(), deleteDoneResult.load());
    }

    // ponytail: định kỳ 5s in nhịp tim chẩn đoán Wi-Fi & MQTT để phát hiện rớt sóng / mất broker
    static unsigned long lastNetDiag = 0;
    if (NET_DIAG_DEBUG && (millis() - lastNetDiag >= 5000)) {
      lastNetDiag = millis();
      int8_t rssi = WiFi.RSSI();
      const char* sigQuality = (rssi > -60) ? "RẤT TỐT" : (rssi > -70) ? "TỐT" : (rssi > -80) ? "YẾU" : "RẤT YẾU/CHẬP CHỜN";
      Serial.printf("[NET-DIAG] WiFi: OK (RSSI: %d dBm [%s], IP: %s) | MQTT: %s (State: %d) | Server: %s:%d\n",
                    rssi, sigQuality, WiFi.localIP().toString().c_str(),
                    mqttClient.connected() ? "CONNECTED" : "DISCONNECTED",
                    mqttClient.state(),
                    serverIP, mqtt_port);
    }
  } else {
    static unsigned long lastWifiDownLog = 0;
    if (NET_DIAG_DEBUG && (millis() - lastWifiDownLog >= 5000)) {
      lastWifiDownLog = millis();
      Serial.printf("[NET-DIAG] ❌ WiFi: DISCONNECTED! Status code: %d. Đang thử kết nối lại...\n", WiFi.status());
    }
  }

#if ENABLE_BLE_FALLBACK
  // 7. Quản lý kênh BLE iBeacon dự phòng:
  // - Nếu Wi-Fi mất kết nối HOẶC Server/MQTT mất kết nối >= 10s: Kích hoạt BLE Scanner.
  // - Nếu Wi-Fi & Server/MQTT đã kết nối tốt: Đóng hoàn toàn BLE để giải phóng RAM & băng thông RF cho Camera/AI.
  static unsigned long serverDisconnectedSince = 0;
  bool isServerConnected = (WiFi.status() == WL_CONNECTED && mqttClient.connected());

  if (isServerConnected) {
    serverDisconnectedSince = 0;
  } else if (serverDisconnectedSince == 0) {
    serverDisconnectedSince = millis();
  }

  bool shouldEnableBle = (!isServerConnected && (WiFi.status() != WL_CONNECTED || (millis() - serverDisconnectedSince >= 10000)));
  handleBleFallback(shouldEnableBle);
#endif

  vTaskDelay(pdMS_TO_TICKS(2)); // Nhường nhẹ CPU
}
