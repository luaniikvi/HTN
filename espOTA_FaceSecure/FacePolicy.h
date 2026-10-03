#pragma once
#include <stdint.h>
#include <math.h>
#include "env.h"

#define CAMERA_ROTATION ROTATION_DEGREE

namespace FacePolicy {
#if CAMERA_ROTATION == 90 || CAMERA_ROTATION == 270
constexpr int IMG_W = 240;
constexpr int IMG_H = 320;
#else
constexpr int IMG_W = 320;
constexpr int IMG_H = 240;
#endif

constexpr int PEOPLE = MAX_FACE;
constexpr int SAMPLES = 3;
constexpr int DIM = 512;
// Cosine similarity, NOT probability. Calibrated for ESP-DL INT8 quantized embeddings.
constexpr float MATCH = MATCH_THRESHOLD;
constexpr float SUPPORT = SUPPORT_THRESHOLD;
constexpr float MARGIN = 0.08f;
constexpr float ENROLL_CONSISTENCY = 0.43f;
constexpr uint32_t MAX_GAP_MS = 3500;
constexpr uint32_t FRAME_MAX_AGE_MS = 2500;

inline float dot(const float *a, const float *b) {
  float s = 0;
  for (int i = 0; i < DIM; ++i) s += a[i] * b[i];
  return s;
}
inline bool normalize(float *a) {
  float n = dot(a, a);
  if (!isfinite(n) || n < 1e-8f) return false;
  n = 1.0f / sqrtf(n);
  for (int i = 0; i < DIM; ++i) a[i] *= n;
  return true;
}
struct Decision { int id; float best; float runner; float support; };
inline bool accepted(const Decision &d) {
  return d.id > 0 && isfinite(d.best) && isfinite(d.runner) &&
         isfinite(d.support) && d.best >= MATCH && d.support >= SUPPORT &&
         d.best - d.runner >= MARGIN;
}
struct Consecutive {
  int id = -1, hits = 0;
  uint32_t last = 0, epoch = 0;
  void reset() { id = -1; hits = 0; last = 0; }
  bool feed(int candidate, uint32_t now, uint32_t generation) {
    if (generation != epoch) { reset(); epoch = generation; }
    if (candidate <= 0) {
      if (last > 0 && uint32_t(now - last) > MAX_GAP_MS) reset();
      return false;
    }
    if (hits && candidate == id && generation == epoch && now == last) return false;
    if (candidate != id || (last > 0 && uint32_t(now - last) > MAX_GAP_MS)) {
      id = candidate; hits = 0;
    }
    last = now;
    return ++hits >= CONFIRM_FRAMES;
  }
};
}
