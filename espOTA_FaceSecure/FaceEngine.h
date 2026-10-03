#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <esp_heap_caps.h>
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "face_recognition_112_v1_s8.hpp"
#include "FacePolicy.h"

// All methods are called ONLY by the inference task. No model/NVS access in MQTT callbacks.
class FaceEngine {
 public:
  struct Person { int32_t id; float sample[FacePolicy::SAMPLES][FacePolicy::DIM]; };
  struct Database {
    uint32_t magic, version, nextId;
    Person people[FacePolicy::PEOPLE];
  };
  Database *db = nullptr;
  bool ready = false;
  const char *reason = "STARTING";
  float confidence = 0;
  int box[4] = {};
  // Reference detector order: eye-left, mouth-left, nose, eye-right, mouth-right.
  float yaw = 0;
  float meanL = 0, sharpL = 0;

  bool begin() {
    if (!psramFound()) { reason = "NO_PSRAM"; return false; }
    db = (Database*)heap_caps_calloc(1, sizeof(Database), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!db) { reason = "NO_DB_MEMORY"; return false; }
    // Never automatically erase a corrupt partition: fail closed.
    if (nvs_flash_init_partition("face_store") != ESP_OK ||
        !prefs.begin("faces_v1", false, "face_store")) {
      reason = "FACE_STORAGE_ERROR"; return false;
    }
    size_t len = prefs.getBytesLength("database");
    if (len) {
      if (len != sizeof(Database) || prefs.getBytes("database", db, len) != len || !valid()) {
        reason = "INVALID_DATABASE"; return false;
      }
      activeFacesMask.store(prefs.getUInt("active_mask", 0xFFFFFFFF));
    } else {
      db->magic = 0x46534331; db->version = 1; db->nextId = 1;
      if (!save()) { reason = "FLASH_WRITE_FAILED"; return false; }
      activeFacesMask.store(0xFFFFFFFF);
      prefs.putUInt("active_mask", 0xFFFFFFFF);
    }
    ready = true; reason = "READY"; return true;
  }
  static inline std::atomic<uint32_t> activeFacesMask{0xFFFFFFFF};
  static void setFaceActive(int id, bool active) {
    if (id <= 0 || id > 31) return;
    uint32_t mask = activeFacesMask.load();
    if (active) mask |= (1UL << id);
    else mask &= ~(1UL << id);
    activeFacesMask.store(mask);
    Preferences p;
    if (p.begin("faces_v1", false, "face_store")) {
      p.putUInt("active_mask", mask);
      p.end();
    }
  }
  int count() const {
    int n = 0; if (db) for (auto &p : db->people) if (p.id > 0) ++n;
    return n;
  }
  bool erase(int id) {
    if (!ready) return false;
    bool found = id == -1;
    // Stage mutation in another PSRAM buffer. Publish RAM state only after NVS succeeds.
    Database *next = (Database*)heap_caps_malloc(sizeof(Database), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!next) return false;
    memcpy(next, db, sizeof(Database));
    for (auto &p : next->people) if (id == -1 || p.id == id) { found = true; memset(&p, 0, sizeof(p)); }
    bool ok = found && prefs.putBytes("database", next, sizeof(Database)) == sizeof(Database);
    if (ok) memcpy(db, next, sizeof(Database));
    free(next); return ok;
  }
  int add(const float samples[FacePolicy::SAMPLES][FacePolicy::DIM]) {
    if (!ready || count() >= FacePolicy::PEOPLE || db->nextId >= 0x7fffffffU) return -1;
    Database *next = (Database*)heap_caps_malloc(sizeof(Database), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!next) return -1;
    memcpy(next, db, sizeof(Database));
    int id = -1;
    for (auto &p : next->people) if (!p.id) {
      id = p.id = next->nextId++;
      memcpy(p.sample, samples, sizeof(p.sample)); break;
    }
    bool ok = prefs.putBytes("database", next, sizeof(Database)) == sizeof(Database);
    if (ok) memcpy(db, next, sizeof(Database));
    free(next); return ok ? id : -1;
  }
  FacePolicy::Decision match(const float *emb) const {
    FacePolicy::Decision result{-1, -1.0f, -1.0f, -1.0f};
    uint32_t mask = activeFacesMask.load();
    for (const auto &p : db->people) if (p.id > 0 && (p.id > 31 || (mask & (1UL << p.id)))) {
      float best = -1, second = -1;
      for (const auto &s : p.sample) {
        float v = FacePolicy::dot(emb, s);
        if (v > best) { second = best; best = v; }
        else if (v > second) second = v;
      }
      if (best > result.best) {
        result.runner = result.best; result.best = best;
        result.support = second; result.id = p.id;
      } else if (best > result.runner) result.runner = best;
    }
    return result;
  }
  bool extract(uint8_t *pixels, float *embedding) {
    reason = "NO_FACE";
    auto &candidates = detector.infer(pixels, {FacePolicy::IMG_H, FacePolicy::IMG_W, 3});
    auto &faces = refine.infer(pixels, {FacePolicy::IMG_H, FacePolicy::IMG_W, 3}, candidates);
    if (faces.size() != 1) {
      if (faces.size() > 1) reason = "MULTIPLE_FACES";
      confidence = 0; box[0] = box[1] = box[2] = box[3] = 0; yaw = 0; meanL = 0; sharpL = 0;
      return false;
    }
    auto &f = faces.front();
    confidence = f.score;
    if (f.box.size() < 4 || f.keypoint.size() != 10 || !isfinite(f.score) || f.score < 0.62f) {
      reason = "LOW_DETECTION_CONFIDENCE"; return false;
    }
    for (int i=0; i<4; ++i) box[i]=f.box[i];
    int w = box[2]-box[0], h = box[3]-box[1];
    if (box[0]<0 || box[1]<0 || box[2]>(FacePolicy::IMG_W-1) || box[3]>(FacePolicy::IMG_H-1) ||
        w<40 || h<40 || w>(FacePolicy::IMG_W-40) || h>(FacePolicy::IMG_H-40)) {
      reason = "FACE_SIZE_OR_CROPPED"; return false;
    }
    for (int i=0; i<10; i+=2) {
      if (f.keypoint[i]<box[0] || f.keypoint[i]>box[2] ||
          f.keypoint[i+1]<box[1] || f.keypoint[i+1]>box[3]) {
        reason = "INVALID_LANDMARKS"; return false;
      }
    }
    // Order verified against core 2.0.17 CameraWebServer: left eye,
    // left mouth, nose, right eye, right mouth; each is an (x,y) pair.
    const int e0=0, e1=6, n=4;
    float eyeDx=abs(f.keypoint[e0]-f.keypoint[e1]);
    if (eyeDx<16 || abs(f.keypoint[e0+1]-f.keypoint[e1+1])>eyeDx*0.48f) {
      reason = "POSE_OR_TOO_SMALL"; return false;
    }
    yaw=(f.keypoint[n]-(f.keypoint[e0]+f.keypoint[e1])*0.5f)/eyeDx;
    if (fabsf(yaw)>YAW_ABS) { reason="TOO_SIDEWAYS"; return false; }
    // Cheap ROI quality measurement BEFORE the expensive embedding model.
    float sum=0, sq=0, lapSum=0, lapSq=0; int count=0, clipped=0;
    auto lum=[pixels](int x,int y)->int {
      const uint8_t *p=pixels+3*(y*FacePolicy::IMG_W+x);
      // fmt2rgb888's legacy output is BGR, matching ESP-DL's Tensor input.
      return (29*p[0]+150*p[1]+77*p[2])>>8;
    };
    int y0 = box[1] + (box[3] - box[1]) * 0.15f;
    int y1 = box[3] - (box[3] - box[1]) * 0.10f;
    int x0 = box[0] + (box[2] - box[0]) * 0.12f;
    int x1 = box[2] - (box[2] - box[0]) * 0.12f;
    for(int y=y0;y<y1;y+=3) for(int x=x0;x<x1;x+=3) {
      int v=lum(x,y), l=4*v-lum(x-1,y)-lum(x+1,y)-lum(x,y-1)-lum(x,y+1);
      sum+=v; sq+=v*v; lapSum+=l; lapSq+=l*l; ++count;
      if(v<4 || v>252) ++clipped;
    }
    if (count == 0) { reason = "BAD_LIGHT"; return false; }
    float mean=sum/count, variance=sq/count-mean*mean;
    float sharp=lapSq/count-(lapSum/count)*(lapSum/count);
    meanL = mean; sharpL = sharp;
    if(mean<16 || mean>235 || clipped>count*0.65f) { reason="BAD_LIGHT"; return false; }
    if(variance<50 || sharp<10) { reason="BLUR_OR_LOW_CONTRAST"; return false; }
    dl::Tensor<uint8_t> input;
    input.set_element(pixels).set_shape({FacePolicy::IMG_H, FacePolicy::IMG_W, 3}).set_auto_free(false);
    // The legacy API has no public extract-only call. Temporary RAM enrollment
    // performs alignment + inference even with an empty gallery. Never writes flash.
    int temporaryId = recognizer.enroll_id(input, f.keypoint, "", false);
    if (temporaryId < 0) { reason="EMBEDDING_ERROR"; return false; }
    auto &emb = recognizer.get_face_emb(temporaryId);
    bool ok = emb.element && emb.get_size() == FacePolicy::DIM;
    if(ok) memcpy(embedding, emb.element, sizeof(float)*FacePolicy::DIM);
    recognizer.clear_id(false);
    if(!ok || !FacePolicy::normalize(embedding)) { reason="INVALID_EMBEDDING"; return false; }
    reason="OK"; return true;
  }
 private:
  Preferences prefs;
  HumanFaceDetectMSR01 detector{0.30f, 0.30f, 10, 0.30f};
  HumanFaceDetectMNP01 refine{0.65f, 0.30f, 10};
  FaceRecognition112V1S8 recognizer;
  bool save() { return prefs.putBytes("database", db, sizeof(Database)) == sizeof(Database); }
  bool valid() {
    if(db->magic!=0x46534331 || db->version!=1 || db->nextId<1 || db->nextId>=0x7fffffffU) return false;
    for(int i=0;i<FacePolicy::PEOPLE;++i) {
      auto &p=db->people[i];
      if(p.id<0 || uint32_t(p.id)>=db->nextId) return false;
      if(!p.id) continue;
      for(int j=0;j<i;++j) if(db->people[j].id==p.id) return false;
      for(auto &s:p.sample) {
        float norm=FacePolicy::dot(s,s);
        if(!isfinite(norm) || norm<0.95f || norm>1.05f) return false;
      }
    }
    return true;
  }
};
