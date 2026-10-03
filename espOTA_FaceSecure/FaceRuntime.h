#pragma once
#include "FaceEngine.h"

// Single bounded JPEG mailbox: capture never waits for inference.
static constexpr size_t AI_JPEG_CAPACITY = 96*1024;
static uint8_t *aiJpeg = nullptr;
static size_t aiJpegLength = 0;
static uint32_t aiFrameMillis = 0, aiFrameEpoch = 0;
static bool aiFrameEnrolling = false;
static std::atomic<bool> aiBusy{false}, aiReady{false}, otaStopping{false};
static std::atomic<int> enrolledFaces{0};
static std::atomic<bool> wsInitRequested{false};
static TaskHandle_t inferenceHandle = nullptr;
static QueueHandle_t deleteQueue = nullptr;
static std::atomic<bool> deletePending{false};
static TaskHandle_t uploadHandle = nullptr;
static uint8_t *breachJpeg = nullptr;
static size_t breachLength = 0;
static std::atomic<bool> uploadBusy{false};

static void breachUploadTask(void *) {
  for(;;) {
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    if(!otaStopping && WiFi.status()==WL_CONNECTED && strlen(breach_upload_url)>0) {
      HTTPClient http;http.setConnectTimeout(800);http.setTimeout(1500);
      if(http.begin(breach_upload_url)) {
        http.addHeader("Content-Type","image/jpeg");http.addHeader("X-Device-Id",DEVICE_ID);http.addHeader("X-Mode","ARMED");
        Serial.printf("[BREACH] HTTP %d\n",http.POST(breachJpeg,breachLength));http.end();
      }
    }
    uploadBusy=false;
  }
}

static int getEnrolledFacesCount() { return enrolledFaces.load(); }

static bool postAI(AIEventType type, int id, int step, uint32_t epoch,
                   uint32_t frameTime, const char *reason = "") {
  AIEventMsg msg{};
  msg.type=type; msg.face_id=id; msg.step=step;
  msg.epoch=epoch; msg.frameMillis=frameTime;
  snprintf(msg.reason,sizeof(msg.reason),"%s",reason);
  return xQueueSend(aiEventQueue,&msg,pdMS_TO_TICKS(50))==pdTRUE;
}

#if CAMERA_ROTATION == 90
static void rotate90CW(const uint8_t *src, uint8_t *dst, int src_w, int src_h) {
  for (int y = 0; y < src_h; ++y) {
    const uint8_t *src_row = src + y * src_w * 3;
    for (int x = 0; x < src_w; ++x) {
      int dst_x = (src_h - 1) - y;
      int dst_y = x;
      int dst_idx = (dst_y * src_h + dst_x) * 3;
      const uint8_t *s = src_row + x * 3;
      dst[dst_idx]     = s[0];
      dst[dst_idx + 1] = s[1];
      dst[dst_idx + 2] = s[2];
    }
  }
}
#elif CAMERA_ROTATION == 270
static void rotate90CCW(const uint8_t *src, uint8_t *dst, int src_w, int src_h) {
  for (int y = 0; y < src_h; ++y) {
    const uint8_t *src_row = src + y * src_w * 3;
    for (int x = 0; x < src_w; ++x) {
      int dst_x = y;
      int dst_y = (src_w - 1) - x;
      int dst_idx = (dst_y * src_h + dst_x) * 3;
      const uint8_t *s = src_row + x * 3;
      dst[dst_idx]     = s[0];
      dst[dst_idx + 1] = s[1];
      dst[dst_idx + 2] = s[2];
    }
  }
}
#endif

static void inferenceTask(void *) {
  FaceEngine *engine = new FaceEngine();
  uint8_t *rgb=(uint8_t*)heap_caps_malloc(320*240*3,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#if CAMERA_ROTATION != 0
  uint8_t *rgbRotated=(uint8_t*)heap_caps_malloc(FacePolicy::IMG_W*FacePolicy::IMG_H*3,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#endif
  float *embedding=(float*)heap_caps_malloc(FacePolicy::DIM*sizeof(float),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  float (*staged)[FacePolicy::DIM]=(float(*)[FacePolicy::DIM])heap_caps_calloc(
      FacePolicy::SAMPLES,FacePolicy::DIM*sizeof(float),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  float previous[FacePolicy::DIM]{};
#if CAMERA_ROTATION != 0
  bool ok=rgb && rgbRotated && embedding && staged && engine && engine->begin();
#else
  bool ok=rgb && embedding && staged && engine && engine->begin();
#endif
  if(!ok) {
    Serial.printf("[FACE] DISABLED: %s\n",engine?engine->reason:"NO_MEMORY");
    aiReady=false;
    // Retain task to return explicit failures to web commands.
  } else {
    enrolledFaces=engine->count(); aiReady=true;
    updateSharedEnrolledFaces(engine);
    Serial.printf("[FACE] READY: %d/10 people; real ESP-DL INT8 embeddings (ROTATION=%ddeg %dx%d)\n",
                  engine->count(), CAMERA_ROTATION, FacePolicy::IMG_W, FacePolicy::IMG_H);
  }
  FacePolicy::Consecutive votes;
  uint32_t session=0, started=0, lastSample=0, completed=0, lastLog=0;
  int step=0, stable=0, side=0;
  int lastBox[4]{};
  for(;;) {
    if(otaStopping) { aiBusy=false; vTaskDelay(pdMS_TO_TICKS(20)); continue; }
    int deleteId;
    if(xQueueReceive(deleteQueue,&deleteId,0)==pdTRUE) {
      bool deleted=ok && engine->erase(deleteId);
      if(ok) {
        enrolledFaces=engine->count();
        updateSharedEnrolledFaces(engine);
      }
      xSemaphoreTake(sharedStateMutex,portMAX_DELAY);
      ++authEpoch; isAuthenticated=false;
      xSemaphoreGive(sharedStateMutex);
      votes.reset(); step=stable=0;
      postAI(AI_EVT_DELETE_DONE,deleteId,deleted?1:0,0,millis(),deleted?"":"NOT_FOUND_OR_STORAGE_ERROR");
      deletePending=false;
    }
    uint32_t epoch=authEpoch.load();
    if(isEnrolling && session!=epoch) {
      session=epoch; started=millis(); lastSample=0; step=stable=side=0; votes.reset();
    }
    if(isEnrolling && completed!=epoch && (!ok || enrolledFaces>=FacePolicy::PEOPLE || millis()-started>60000)) {
      postAI(AI_EVT_ENROLL_FAILED,0,0,epoch,millis(),!ok?"AI_NOT_READY":(enrolledFaces>=FacePolicy::PEOPLE?"FULL_PEOPLE":"TIMEOUT"));
      completed=epoch;
    }
    if(ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(25))==0) continue;
    const uint32_t frameEpoch=aiFrameEpoch, stamp=aiFrameMillis;
    const bool enrolling=aiFrameEnrolling;
    uint32_t t0=millis();
    if(!ok || otaStopping || frameEpoch!=authEpoch.load() || deletePending ||
       (enrolling && completed==frameEpoch) || uint32_t(t0-stamp)>FacePolicy::FRAME_MAX_AGE_MS) {
      votes.reset(); aiBusy=false; continue;
    }
#if CAMERA_ROTATION == 90
    bool valid = fmt2rgb888(aiJpeg, aiJpegLength, PIXFORMAT_JPEG, rgb);
    if(valid) {
      rotate90CW(rgb, rgbRotated, 320, 240);
      valid = engine->extract(rgbRotated, embedding);
    }
#elif CAMERA_ROTATION == 270
    bool valid = fmt2rgb888(aiJpeg, aiJpegLength, PIXFORMAT_JPEG, rgb);
    if(valid) {
      rotate90CCW(rgb, rgbRotated, 320, 240);
      valid = engine->extract(rgbRotated, embedding);
    }
#else
    bool valid = fmt2rgb888(aiJpeg, aiJpegLength, PIXFORMAT_JPEG, rgb) && engine->extract(rgb, embedding);
#endif
    // Never allow a pre-command frame to authorize after a mode/cancel/delete change.
    if(frameEpoch!=authEpoch.load() || otaStopping || deletePending ||
       uint32_t(millis()-stamp)>FacePolicy::FRAME_MAX_AGE_MS) {
      votes.reset(); stable=0; aiBusy=false; continue;
    }
    if(!valid) {
      stable=0;
      if(FACE_DEBUG && millis()-lastLog>1200) {
        lastLog=millis();
        Serial.printf("[FACE] reject=%s (conf=%.2f box=[%d,%d,%d,%d] w=%d h=%d yaw=%.2f lum=%.1f sharp=%.1f) inference=%lums\n",
          engine->reason, engine->confidence, engine->box[0], engine->box[1], engine->box[2], engine->box[3],
          engine->box[2]-engine->box[0], engine->box[3]-engine->box[1], engine->yaw, engine->meanL, engine->sharpL, millis()-t0);
      }
      aiBusy=false; continue;
    }
    if(enrolling && isEnrolling) {
      if(session!=frameEpoch) {
        session=frameEpoch; started=millis(); step=stable=side=0; lastSample=0;
        Serial.println("\n[FACE] [ENROLL] ==================================================");
        Serial.println("[FACE] [ENROLL] ▶️ BẮT ĐẦU ĐĂNG KÝ 3 BƯỚC (Giữ mặt trước camera)");
        Serial.println("[FACE] [ENROLL] ==================================================");
      }
      // Bước 1: Bắt buộc nhìn THẲNG vào camera (|yaw| <= 0.18)
      // Bước 2: Bắt buộc XOAY NHẸ sang Trái hoặc Phải (|yaw| >= 0.12 && |yaw| <= 0.48)
      // Bước 3: Bắt buộc XOAY sang HƯỚNG NGƯỢC LẠI so với bước 2 (yaw * side <= -0.10 && |yaw| <= 0.48)
      bool pose = false;
      if (step == 0) {
        pose = (fabsf(engine->yaw) <= 0.18f);
      } else if (step == 1) {
        pose = (fabsf(engine->yaw) >= 0.12f && fabsf(engine->yaw) <= 0.48f);
      } else if (step == 2) {
        pose = (engine->yaw * side <= -0.10f && fabsf(engine->yaw) <= 0.48f);
      }

      bool same = true;
      float minSim = 1.0f;
      for(int i=0; i<step; ++i) {
        float sim = FacePolicy::dot(staged[i], embedding);
        if(sim < minSim) minSim = sim;
        if(sim < FacePolicy::ENROLL_CONSISTENCY) same = false;
      }
      uint32_t elapsedSinceLast = millis() - lastSample;
      bool cooldownOk = (step == 0) || (elapsedSinceLast >= 500);

      if(!pose || !same || !cooldownOk) {
        if(millis() - lastLog > 500) {
          lastLog = millis();
          String waitReason = "";
          if(!pose) {
            if(step == 0) waitReason += "Cần nhìn THẲNG vào cam (|yaw|<=0.18) ";
            else if(step == 1) waitReason += "Cần XOAY NHẸ sang Trái hoặc Phải (|yaw|>=0.12) ";
            else waitReason += (side > 0 ? "Cần XOAY sang TRÁI (ngược lại bước 2) " : "Cần XOAY sang PHẢI (ngược lại bước 2) ");
          }
          if(!same) waitReason += String("Không khớp người (sim=") + minSim + "<0.43) ";
          if(!cooldownOk) waitReason += String("Đang chờ ") + (500 - elapsedSinceLast) + "ms ";
          Serial.printf("[FACE] [ENROLL] ⏳ Bước %d/3: yaw=%.2f | lum=%.1f sharp=%.1f | %s\n",
            step + 1, engine->yaw, engine->meanL, engine->sharpL, waitReason.c_str());
        }
      } else {
        auto match = engine->match(embedding);
        if(step == 0 && match.best >= FacePolicy::MATCH) {
          Serial.printf("[FACE] [ENROLL] ❌ BỊ TRÙNG: Đã tồn tại trong hệ thống (ID: %d, cos=%.3f >= %.2f)\n",
            match.id, match.best, FacePolicy::MATCH);
          postAI(AI_EVT_ENROLL_FAILED, match.id, 0, frameEpoch, millis(), "ALREADY_ENROLLED");
          completed = frameEpoch;
        } else {
          memcpy(staged[step], embedding, sizeof(staged[step]));
          if (step == 1) {
            side = (engine->yaw > 0) ? 1 : -1; // Ghi nhớ hướng xoay của bước 2
          }
          ++step;
          lastSample = millis();
          const char *stepDesc = (step == 1) ? "Mặt thẳng" :
                                 (step == 2 ? (side > 0 ? "Góc Phải" : "Góc Trái") :
                                              (side > 0 ? "Góc Trái" : "Góc Phải"));
          Serial.printf("[FACE] [ENROLL] ✅ THÀNH CÔNG BƯỚC %d/3! (%s, yaw=%.2f, lum=%.1f, sharp=%.1f%s)\n",
            step, stepDesc, engine->yaw, engine->meanL, engine->sharpL,
            step > 1 ? (String(", sim=")+minSim).c_str() : "");
          if(step == 1) {
            Serial.println("[FACE] [ENROLL] 👉 Tiếp theo Bước 2/3: Hãy xoay nhẹ mặt sang TRÁI hoặc PHẢI (~15-20 độ)");
          } else if(step == 2) {
            Serial.printf("[FACE] [ENROLL] 👉 Tiếp theo Bước 3/3: Hãy xoay mặt sang HƯỚNG NGƯỢC LẠI (%s)\n",
              side > 0 ? "TRÁI" : "PHẢI");
          }
          postAI(AI_EVT_ENROLL_STEP_OK, 0, step, frameEpoch, millis());
          if(step == 3) {
            // Serialize final commit with cancel/mode commands.
            xSemaphoreTake(sharedStateMutex, portMAX_DELAY);
            int id = -1;
            if(frameEpoch == authEpoch.load() && isEnrolling && !otaStopping) {
              id = engine->add(staged);
              if(id > 0) { isEnrolling = false; enrollStep = 0; }
            }
            xSemaphoreGive(sharedStateMutex);
            enrolledFaces = engine->count();
            if(id > 0) updateSharedEnrolledFaces(engine);
            completed = frameEpoch;
            if(id > 0) {
              Serial.println("\n[FACE] [ENROLL] ==================================================");
              Serial.printf("[FACE] [ENROLL] 🎉 HOÀN TẤT ĐĂNG KÝ! Lưu thành công ID=%d (Đủ 3 góc Thẳng/Trái/Phải, Tổng: %d người)\n", id, enrolledFaces.load());
              Serial.println("[FACE] [ENROLL] ==================================================\n");
            } else {
              Serial.println("[FACE] [ENROLL] ❌ LỖI LƯU FLASH NVS HOẶC BỊ HỦY LỆNH!");
            }
            postAI(id > 0 ? AI_EVT_ENROLL_FINISHED : AI_EVT_ENROLL_FAILED, id, 3, frameEpoch, millis(), id > 0 ? "" : "SAVE_FAILED_OR_CANCELLED");
          }
        }
      }
    } else if(!isEnrolling && currentMode!=MODE_DISARMED && !isAuthenticated && millis() >= authCooldownMillis.load()) {
      auto d=engine->match(embedding);
      int id=FacePolicy::accepted(d)?d.id:-1;
      memcpy(lastBox,engine->box,sizeof(lastBox));
      bool confirmed=votes.feed(id,millis(),frameEpoch);
      if(FACE_DEBUG) {
        Serial.printf("[FACE] [AUTH] ID=%d | cos=%.3f (cần >=%.2f) | support=%.3f (cần >=%.2f) | runner=%.3f | hits=%d/%d | conf=%.2f yaw=%.2f | inf=%lums\n",
            d.id, d.best, FacePolicy::MATCH, d.support, FacePolicy::SUPPORT, d.runner, votes.hits, CONFIRM_FRAMES,
            engine->confidence, engine->yaw, millis()-t0);
      }
      if(confirmed) {
        Serial.printf("[FACE] [AUTH] 🔓 XÁC THỰC THÀNH CÔNG! Chấp nhận người dùng ID=%d (Đã đạt %d/%d hits)\n", id, votes.hits, CONFIRM_FRAMES);
        postAI(AI_EVT_AUTH_SUCCESS,id,0,frameEpoch,stamp);
        votes.reset();
        aiJpegLength = 0;
        memset(embedding, 0, sizeof(float) * FacePolicy::DIM);
      }
    } else votes.reset();
    aiBusy=false;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// Dedicated Non-Blocking Stream Mailbox: DMA SRAM buffer for zero-copy Wi-Fi transmission
static constexpr size_t STREAM_JPEG_CAPACITY = 16 * 1024;
static uint8_t *streamJpeg = nullptr;
static size_t streamJpegLen = 0;
static std::atomic<bool> streamBusy{false};
static TaskHandle_t streamSenderHandle = nullptr;
static uint32_t streamFrames = 0, streamSentBytes = 0, streamDropsCount = 0;

static void streamSenderTask(void *) {
  bool wsStarted = false;
  for(;;) {
    // Timeout ngắn 5ms để webSocket.loop() được xử lý liên tục, không ứ đọng TCP buffer
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5));
    if(otaStopping) { streamBusy = false; continue; }

    if(wsInitRequested.exchange(false)) {
      webSocket.begin(serverIP, ws_port, ws_path);
      webSocket.onEvent(webSocketEvent);
      webSocket.setReconnectInterval(2000);
      webSocket.enableHeartbeat(15000, 4000, 2);
      wsStarted = true;
    }
    if(wsStarted && WiFi.status() == WL_CONNECTED) {
      webSocket.loop();
    }

    if(streamBusy.load()) {
      if(WiFi.status() == WL_CONNECTED && webSocket.isConnected() && streamJpegLen > 0) {
        if(webSocket.sendBIN(streamJpeg, streamJpegLen)) {
          ++streamFrames;
          streamSentBytes += streamJpegLen;
        } else {
          ++streamDropsCount;
        }
        webSocket.loop();
      }
      streamBusy = false;
    }
  }
}

void aiCameraTask(void *) {
  uint32_t lastScan = 0, lastStream = 0, statsStart = millis(), submitted = 0;
  bool prevWantAI = false;
  for(;;) {
    if(otaStopping) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

    bool enrolling = isEnrolling.load();
    bool streaming = streamEnabled.load();
    bool auth = isAuthenticated.load();
    SystemMode mode = currentMode.load();
    uint32_t epoch = authEpoch.load();
    bool wantAI = aiReady && !deletePending && (enrolling || (!auth && mode!=MODE_DISARMED && enrolledFaces>0));
    bool wantStream = streaming && WiFi.status()==WL_CONNECTED && webSocket.isConnected();
    bool breach = captureBreachRequested.exchange(false);

    if(!wantAI && !wantStream && !breach) { prevWantAI = false; vTaskDelay(pdMS_TO_TICKS(15)); continue; }
    if(!wantStream && !breach && (aiBusy.load() || millis()-lastScan<200)) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

    // Khi AI vừa kích hoạt lại (sau timeout, mở khóa hoặc đổi chế độ): xả sạch frame cũ đọng trong DMA camera!
    if(wantAI && !prevWantAI) {
      for(int i = 0; i < 2; ++i) {
        camera_fb_t *staleFb = esp_camera_fb_get();
        if(staleFb) esp_camera_fb_return(staleFb);
      }
      lastScan = millis();
    }
    prevWantAI = wantAI;

    camera_fb_t *fb = esp_camera_fb_get();
    if(!fb) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }

    uint32_t now = millis();
    uint32_t stamp = now; // Luôn dùng monotonic millis() đồng bộ với hệ thống
    bool fresh = true;

    // 1. Phục vụ AI Face Recognition (~5 fps, 200ms/lần)
    if(wantAI && fresh && fb->format==PIXFORMAT_JPEG && fb->width==320 && fb->height==240 &&
       fb->len>0 && fb->len<=AI_JPEG_CAPACITY && (now-lastScan>=200) && !aiBusy.exchange(true)) {
      memcpy(aiJpeg, fb->buf, fb->len); aiJpegLength = fb->len;
      aiFrameMillis = stamp; aiFrameEpoch = epoch; aiFrameEnrolling = enrolling;
      lastScan = now; ++submitted; xTaskNotifyGive(inferenceHandle);
    }

    // 2. Phục vụ WebSocket Live Stream (Zero-Blocking: Copy mailbox trong SRAM DMA, nhịp 50ms ~ 20 FPS)
    if(wantStream && fb->format==PIXFORMAT_JPEG && fb->len>0 && fb->len<=STREAM_JPEG_CAPACITY) {
      if(now-lastStream >= 50 && !streamBusy.exchange(true)) {
        lastStream = now;
        memcpy(streamJpeg, fb->buf, fb->len);
        streamJpegLen = fb->len;
        xTaskNotifyGive(streamSenderHandle);
      }
    }

    // 3. Phục vụ Breach Capture Snapshot
    if(breach && fb->format==PIXFORMAT_JPEG && fb->len<=AI_JPEG_CAPACITY &&
       !uploadBusy.exchange(true)) {
      memcpy(breachJpeg, fb->buf, fb->len); breachLength = fb->len;
      xTaskNotifyGive(uploadHandle);
    }

    esp_camera_fb_return(fb);

    if(now - statsStart >= 3000) {
      if(PERF_DEBUG) {
        float seconds = (now - statsStart) / 1000.0f;
        float fps = streamFrames / seconds;
        float kbps = (streamSentBytes * 8.0f) / (seconds * 1024.0f);
        float kBps = streamSentBytes / (seconds * 1024.0f);
        Serial.printf("[PERF] stream=%.1ffps | bw=%.1f KB/s (%.1f kbps) | drops=%u | AI=%.1f sub/s | WiFi RSSI=%d dBm | WS=%s | heap=%u | psram=%u\n",
            fps, kBps, kbps, streamDropsCount, submitted/seconds, WiFi.RSSI(),
            webSocket.isConnected() ? "CONNECTED" : "DISCONNECTED",
            ESP.getFreeHeap(), ESP.getFreePsram());
        if (streaming && !webSocket.isConnected()) {
          Serial.printf("[STREAM-WARN] ⚠️ Stream đang bật nhưng WebSocket chưa kết nối tới %s:%d! WiFi: %s\n",
                        serverIP, ws_port, WiFi.status() == WL_CONNECTED ? "OK" : "DISCONNECTED");
        }
      }
      streamFrames = submitted = streamSentBytes = streamDropsCount = 0; statsStart = now;
    }

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

static bool startFaceTasks() {
  aiJpeg=(uint8_t*)heap_caps_malloc(AI_JPEG_CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  breachJpeg=(uint8_t*)heap_caps_malloc(AI_JPEG_CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  // streamJpeg cấp phát trong INTERNAL SRAM có gắn cờ DMA cho Wi-Fi hardware
  streamJpeg=(uint8_t*)heap_caps_malloc(STREAM_JPEG_CAPACITY,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
  if(!streamJpeg) streamJpeg=(uint8_t*)malloc(STREAM_JPEG_CAPACITY); // fallback nếu heap DMA bận
  deleteQueue=xQueueCreate(4,sizeof(int));
  if(!aiJpeg || !breachJpeg || !streamJpeg || !deleteQueue) return false;
  if(xTaskCreatePinnedToCore(breachUploadTask,"breachUpload",8192,nullptr,1,&uploadHandle,0)!=pdPASS) return false;
  if(xTaskCreatePinnedToCore(inferenceTask,"faceInference",16384,nullptr,1,&inferenceHandle,1)!=pdPASS) return false;
  if(xTaskCreatePinnedToCore(streamSenderTask,"wsStreamSender",8192,nullptr,1,&streamSenderHandle,0)!=pdPASS) return false;
  // aiCameraTask chạy trên Core 1 chuyên trách cảm biến/camera/AI, giải phóng Core 0 cho Wi-Fi & lwIP
  return xTaskCreatePinnedToCore(aiCameraTask,"cameraStream",8192,nullptr,1,&aiCameraTaskHandle,1)==pdPASS;
}
