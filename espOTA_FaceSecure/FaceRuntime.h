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

static void inferenceTask(void *) {
  FaceEngine *engine = new FaceEngine();
  uint8_t *rgb=(uint8_t*)heap_caps_malloc(320*240*3,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  float *embedding=(float*)heap_caps_malloc(FacePolicy::DIM*sizeof(float),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  float (*staged)[FacePolicy::DIM]=(float(*)[FacePolicy::DIM])heap_caps_calloc(
      FacePolicy::SAMPLES,FacePolicy::DIM*sizeof(float),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  float previous[FacePolicy::DIM]{};
  bool ok=rgb && embedding && staged && engine && engine->begin();
  if(!ok) {
    Serial.printf("[FACE] DISABLED: %s\n",engine?engine->reason:"NO_MEMORY");
    aiReady=false;
    // Retain task to return explicit failures to web commands.
  } else {
    enrolledFaces=engine->count(); aiReady=true;
    Serial.printf("[FACE] READY: %d/10 people; real ESP-DL INT8 embeddings\n",engine->count());
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
      if(ok) enrolledFaces=engine->count();
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
    if(isEnrolling && completed!=epoch && (!ok || enrolledFaces>=10 || millis()-started>60000)) {
      postAI(AI_EVT_ENROLL_FAILED,0,0,epoch,millis(),!ok?"AI_NOT_READY":(enrolledFaces>=10?"FULL_10_PEOPLE":"TIMEOUT"));
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
    bool valid=fmt2rgb888(aiJpeg,aiJpegLength,PIXFORMAT_JPEG,rgb) && engine->extract(rgb,embedding);
    // Never allow a pre-command frame to authorize after a mode/cancel/delete change.
    if(frameEpoch!=authEpoch.load() || otaStopping || deletePending ||
       uint32_t(millis()-stamp)>FacePolicy::FRAME_MAX_AGE_MS) {
      votes.reset(); stable=0; aiBusy=false; continue;
    }
    if(!valid) {
      votes.reset(); stable=0;
      if(millis()-lastLog>1500) { lastLog=millis(); Serial.printf("[FACE] reject=%s inference=%lums\n",engine->reason,millis()-t0); }
      aiBusy=false; continue;
    }
    if(enrolling && isEnrolling) {
      if(session!=frameEpoch) {session=frameEpoch;started=millis();step=stable=side=0;lastSample=0;}
      bool pose=step==0 ? fabsf(engine->yaw)<=0.14f :
        (step==1 ? fabsf(engine->yaw)>=0.06f && fabsf(engine->yaw)<=0.42f :
          engine->yaw*side<=-0.06f && fabsf(engine->yaw)<=0.42f);
      bool same=true;
      for(int i=0;i<step;++i) if(FacePolicy::dot(staged[i],embedding)<FacePolicy::ENROLL_CONSISTENCY) same=false;
      if(!pose || !same || (step>0 && millis()-lastSample<500)) {
        stable=0;
        if(millis()-lastLog>1000) {lastLog=millis();Serial.printf("[ENROLL] waiting step=%d yaw=%.2f same=%d\n",step+1,engine->yaw,same);}
      } else {
        if(stable && FacePolicy::dot(previous,embedding)<0.65f) stable=0;
        memcpy(previous,embedding,sizeof(previous));
        if(++stable>=2) {
          auto match=engine->match(embedding);
          if(step==0 && match.best>=FacePolicy::MATCH) {
            postAI(AI_EVT_ENROLL_FAILED,match.id,0,frameEpoch,millis(),"ALREADY_ENROLLED"); completed=frameEpoch;
          } else {
            memcpy(staged[step],embedding,sizeof(previous));
            if(step==1) side=engine->yaw>0?1:-1;
            ++step;stable=0;lastSample=millis();
            postAI(AI_EVT_ENROLL_STEP_OK,0,step,frameEpoch,millis());
            if(step==3) {
              // Serialize final commit with cancel/mode commands. Only a flash write
              // holds this mutex, never inference or network operations.
              xSemaphoreTake(sharedStateMutex,portMAX_DELAY);
              int id=-1;
              if(frameEpoch==authEpoch.load() && isEnrolling && !otaStopping) {
                id=engine->add(staged);
                if(id>0) { isEnrolling=false; enrollStep=0; }
              }
              xSemaphoreGive(sharedStateMutex);
              enrolledFaces=engine->count(); completed=frameEpoch;
              postAI(id>0?AI_EVT_ENROLL_FINISHED:AI_EVT_ENROLL_FAILED,id,3,frameEpoch,millis(),id>0?"":"SAVE_FAILED_OR_CANCELLED");
            }
          }
        }
      }
    } else if(!isEnrolling && currentMode!=MODE_DISARMED && !isAuthenticated) {
      auto d=engine->match(embedding);
      int id=FacePolicy::accepted(d)?d.id:-1;
      // Reset confirmation on a large jump in the face position/scale.
      if(votes.hits) {
        int oldW=lastBox[2]-lastBox[0], newW=engine->box[2]-engine->box[0];
        if(abs(engine->box[0]-lastBox[0])>oldW/2 || abs(engine->box[1]-lastBox[1])>oldW/2 ||
           newW>oldW*1.6f || newW<oldW*0.6f) votes.reset();
      }
      memcpy(lastBox,engine->box,sizeof(lastBox));
      bool confirmed=votes.feed(id,stamp,frameEpoch);
      Serial.printf("[FACE] id=%d cos=%.3f support=%.3f runner=%.3f hits=%d/%d inference=%lums age=%lums\n",
          d.id,d.best,d.support,d.runner,votes.hits,FacePolicy::CONFIRM_FRAMES,millis()-t0,millis()-stamp);
      if(confirmed) {
        postAI(AI_EVT_AUTH_SUCCESS,id,0,frameEpoch,stamp);
        votes.reset();
      }
    } else votes.reset();
    aiBusy=false;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void aiCameraTask(void *) {
  uint32_t lastScan=0, lastStream=0, statsStart=millis(), frames=0, submitted=0;
  bool wsStarted=false;
  for(;;) {
    if(otaStopping) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
    if(wsInitRequested.exchange(false)) {
      webSocket.begin(serverIP,ws_port,ws_path);
      webSocket.onEvent(webSocketEvent); webSocket.setReconnectInterval(3000);
      wsStarted=true;
    }
    if(wsStarted && WiFi.status()==WL_CONNECTED) webSocket.loop();
    bool enrolling = isEnrolling.load();
    bool streaming = streamEnabled.load();
    bool auth = isAuthenticated.load();
    SystemMode mode = currentMode.load();
    uint32_t epoch = authEpoch.load();
    bool wantAI=aiReady && !deletePending && (enrolling || (!auth && mode!=MODE_DISARMED && enrolledFaces>0));
    bool wantStream=streaming && WiFi.status()==WL_CONNECTED && webSocket.isConnected() && millis()-lastStream>=50;
    bool breach=captureBreachRequested.exchange(false);
    if(!wantAI && !wantStream && !breach) {vTaskDelay(pdMS_TO_TICKS(10));continue;}
    if(!wantStream && !breach && (aiBusy || millis()-lastScan<250)) {vTaskDelay(pdMS_TO_TICKS(5));continue;}
    camera_fb_t *fb=esp_camera_fb_get();
    if(!fb) {vTaskDelay(pdMS_TO_TICKS(10));continue;}
    uint32_t stamp=(uint32_t)(uint64_t(fb->timestamp.tv_sec)*1000ULL+fb->timestamp.tv_usec/1000);
    bool fresh=uint32_t(millis()-stamp)<=FacePolicy::FRAME_MAX_AGE_MS;
    if(wantAI && fresh && fb->format==PIXFORMAT_JPEG && fb->width==320 && fb->height==240 &&
       fb->len>0 && fb->len<=AI_JPEG_CAPACITY && millis()-lastScan>=250 && !aiBusy.exchange(true)) {
      memcpy(aiJpeg,fb->buf,fb->len); aiJpegLength=fb->len;
      aiFrameMillis=stamp;aiFrameEpoch=epoch;aiFrameEnrolling=enrolling;
      lastScan=millis();++submitted;xTaskNotifyGive(inferenceHandle);
    }
    if(wantStream && fb->format==PIXFORMAT_JPEG && fb->len) {
      lastStream=millis();if(webSocket.sendBIN(fb->buf,fb->len))++frames;
    }
    // HTTP upload owns a separate snapshot. Never retain a camera frame while
    // waiting for the backend, nor block inference or the switch/buzzer loop.
    if(breach && fb->format==PIXFORMAT_JPEG && fb->len<=AI_JPEG_CAPACITY &&
       !uploadBusy.exchange(true)) {
      memcpy(breachJpeg,fb->buf,fb->len);breachLength=fb->len;
      xTaskNotifyGive(uploadHandle);
    }
    esp_camera_fb_return(fb);
    if(millis()-statsStart>=5000) {
      float seconds=(millis()-statsStart)/1000.0f;
      Serial.printf("[PERF] stream=%.1ffps AI=%.1f submissions/s heap=%u psram=%u people=%d\n",
          frames/seconds,submitted/seconds,ESP.getFreeHeap(),ESP.getFreePsram(),enrolledFaces.load());
      frames=submitted=0;statsStart=millis();
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

static bool startFaceTasks() {
  aiJpeg=(uint8_t*)heap_caps_malloc(AI_JPEG_CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  breachJpeg=(uint8_t*)heap_caps_malloc(AI_JPEG_CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  deleteQueue=xQueueCreate(4,sizeof(int));
  if(!aiJpeg || !breachJpeg || !deleteQueue) return false;
  if(xTaskCreatePinnedToCore(breachUploadTask,"breachUpload",8192,nullptr,1,&uploadHandle,0)!=pdPASS) return false;
  if(xTaskCreatePinnedToCore(inferenceTask,"faceInference",16384,nullptr,1,&inferenceHandle,1)!=pdPASS) return false;
  return xTaskCreatePinnedToCore(aiCameraTask,"cameraStream",8192,nullptr,1,&aiCameraTaskHandle,0)==pdPASS;
}
