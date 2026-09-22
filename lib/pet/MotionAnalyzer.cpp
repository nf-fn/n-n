#include "MotionAnalyzer.h"

#include <cmath>

namespace pet {
namespace {

// タイムスタンプが飛んだり巻き戻ったりしたときに使う既定の刻み [ms]。
// センサは 100Hz で回す想定。
constexpr uint32_t kDefaultStepMs = 10;
constexpr uint32_t kMaxStepMs = 100;

}  // namespace

void MotionAnalyzer::update(const ImuSample &sample) {
  if (!initialized_) {
    // 最初のサンプルは平滑化せずそのまま採る。
    // 起動直後に顔が中央から流れてくるのを避ける。
    upX_ = sample.ax;
    upY_ = sample.ay;
    upZ_ = sample.az;
    initialized_ = true;
    lastMs_ = sample.tMs;
    recomputePosture();
    return;
  }

  uint32_t stepMs = sample.tMs - lastMs_;
  if (stepMs == 0 || stepMs > kMaxStepMs) {
    // 巻き戻り (uint32 の減算で巨大値になる) と長すぎる欠測をまとめて弾く
    stepMs = kDefaultStepMs;
  }
  lastMs_ = sample.tMs;

  const float dt = static_cast<float>(stepMs) / 1000.0f;
  const float alpha = dt / (kGravityTauSec + dt);

  upX_ += alpha * (sample.ax - upX_);
  upY_ += alpha * (sample.ay - upY_);
  upZ_ += alpha * (sample.az - upZ_);

  recomputePosture();
}

void MotionAnalyzer::recomputePosture() {
  const float mag = std::sqrt(upX_ * upX_ + upY_ * upY_ + upZ_ * upZ_);
  if (mag < 1e-4f) {
    // 自由落下や異常値。前回の姿勢を保つほうが顔が壊れない。
    return;
  }

  const float nx = upX_ / mag;
  const float ny = upY_ / mag;
  const float nz = upZ_ / mag;

  posture_.upX = nx;
  posture_.upY = ny;
  posture_.upZ = nz;

  // 世界の下方向を画面平面へ射影したものが「下り坂」。
  // 単位ベクトルの成分なので長さは自動的に 1 以下に収まる。
  posture_.downX = -nx;
  posture_.downY = -ny;

  posture_.faceUp = nz > kFaceThreshold;
  posture_.faceDown = nz < -kFaceThreshold;
  posture_.inverted = ny < kInvertedThreshold;
}

}  // namespace pet
