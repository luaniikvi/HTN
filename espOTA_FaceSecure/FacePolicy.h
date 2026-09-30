#pragma once
#include <stdint.h>
#include <math.h>

namespace FacePolicy {
constexpr int PEOPLE = 10;
constexpr int SAMPLES = 3;
constexpr int DIM = 512;
// Cosine similarity, NOT probability. Calibrate against real owner/impostor logs.
constexpr float MATCH = 0.70f;
constexpr float SUPPORT = 0.58f;
constexpr float MARGIN = 0.08f;
constexpr float ENROLL_CONSISTENCY = 0.43f;
constexpr int CONFIRM_FRAMES = 3;
constexpr uint32_t MAX_GAP_MS = 1200;
constexpr uint32_t FRAME_MAX_AGE_MS = 1800;

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
    if (candidate <= 0) { reset(); return false; }
    if (hits && candidate == id && generation == epoch && now == last) return false;
    if (candidate != id || generation != epoch || uint32_t(now-last) > MAX_GAP_MS) {
      id = candidate; hits = 0;
    }
    epoch = generation; last = now;
    return ++hits >= CONFIRM_FRAMES;
  }
}
;
}
