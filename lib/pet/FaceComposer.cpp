#include "FaceComposer.h"

#include <cmath>

namespace pet {

uint32_t FaceComposer::rand32() {
  // xorshift32。分布の質は問わない。間隔がばらつけば十分。
  rngState_ ^= rngState_ << 13;
  rngState_ ^= rngState_ >> 17;
  rngState_ ^= rngState_ << 5;
  return rngState_;
}

float FaceComposer::blinkOpenness(uint32_t tMs) {
  const uint32_t span = kBlinkIntervalMaxMs - kBlinkIntervalMinMs + 1;

  if (!blinkScheduled_) {
    blinkStartMs_ = tMs + kBlinkIntervalMinMs + (rand32() % span);
    blinkScheduled_ = true;
  }

  if (tMs < blinkStartMs_) {
    return 1.0f;
  }

  const uint32_t elapsed = tMs - blinkStartMs_;
  if (elapsed < kBlinkDurationMs) {
    // 閉じて開くまでを三角波で表す。0.5 の瞬間に完全に閉じる。
    const float p = static_cast<float>(elapsed) / kBlinkDurationMs;
    return std::fabs(2.0f * p - 1.0f);
  }

  blinkStartMs_ = tMs + kBlinkIntervalMinMs + (rand32() % span);
  return 1.0f;
}

FaceParams FaceComposer::compose(const Posture &posture, uint32_t tMs) {
  FaceParams f;

  // --- 目玉を下り坂へ転がす ---
  float ex = posture.downX * kEyeTravel;
  float ey = posture.downY * kEyeTravel;

  // 顔の輪郭からはみ出さないよう、単位円に収める
  const float mag = std::sqrt(ex * ex + ey * ey);
  if (mag > 1.0f) {
    ex /= mag;
    ey /= mag;
  }
  f.eyeOffsetX = ex;
  f.eyeOffsetY = ey;

  // --- 顔全体を少し傾ける ---
  //
  // 機体が時計回りに theta 傾くと、世界の上は機体座標で反時計回りに回る。
  // 顔は世界の上へ戻ろうとして、その一部だけ反時計回りに回る。
  //
  // 画面が水平に近いと面内の重力成分が消えて向きが定まらないため、
  // 面内成分の大きさを掛けて、水平時に顔が回らないようにする。
  const float inPlaneMag =
      std::sqrt(posture.upX * posture.upX + posture.upY * posture.upY);
  const float deviceTilt = -std::atan2(posture.upX, posture.upY);
  f.faceTiltRad = kTiltFollow * deviceTilt * inPlaneMag;

  // --- まばたき ---
  f.eyeOpen = blinkOpenness(tMs);

  // 口はフェーズ 3 で気分を入れるまで一文字のまま
  f.mouthCurve = 0.0f;

  return f;
}

}  // namespace pet
