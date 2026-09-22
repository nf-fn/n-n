#include "FaceComposer.h"

#include <cmath>

namespace pet {
namespace {

constexpr float kPi = 3.14159265358979f;

// めまいのとき瞳が描く円の速さ [rad/ms] と半径 (正規化座標)
constexpr float kDizzySpinRate = 0.006f;
constexpr float kDizzyRadius = 0.70f;

// 逆さにされたときの困り眉の強さ
constexpr float kInvertedBrow = -0.8f;

float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void approach(float &value, float target, float tau, float dt) {
  if (dt <= 0.0f) {
    return;
  }
  const float alpha = dt / (tau + dt);
  value += alpha * (target - value);
}

}  // namespace

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

FaceParams FaceComposer::compose(const Posture &posture, const MoodState &mood,
                                 uint32_t tMs) {
  FaceParams f;

  // --- 経過時間 ---
  float dt = 0.0f;
  if (timeKnown_ && tMs > lastMs_) {
    dt = static_cast<float>(tMs - lastMs_) / 1000.0f;
    if (dt > 0.1f) {
      dt = 0.1f;  // 長い中断で表情が飛ばないように抑える
    }
  }
  timeKnown_ = true;
  lastMs_ = tMs;

  // --- 視線 ---
  //
  // 既定は姿勢による下り坂。めまいのときだけ上書きする。
  float ex = posture.downX * kEyeTravel;
  float ey = posture.downY * kEyeTravel;

  // --- 表情の目標値を決める ---
  //
  // 強い感情が勝つ。閾値を超えたものが顔を占領する。
  float openTarget = 1.0f;
  float archTarget = 0.0f;
  float browTarget = 0.0f;

  if (mood.dizziness >= kDizzyThreshold) {
    // 目を見開いて、瞳がぐるぐる回る
    openTarget = 1.0f;
    const float angle = static_cast<float>(tMs) * kDizzySpinRate;
    const float radius = kDizzyRadius * mood.dizziness;
    ex = std::cos(angle) * radius;
    ey = std::sin(angle) * radius;

  } else if (mood.anger >= kAngerThreshold) {
    // 眉を出して内側を下げる。目は少し細める。
    openTarget = 0.80f;
    browTarget = mood.anger;

  } else if (mood.sleepiness >= kSleepyThreshold) {
    // 眠いほど目が閉じ、閉じた目は深い下向きの弧になる
    openTarget = clampf(1.0f - mood.sleepiness, 0.04f, 1.0f);
    archTarget = -mood.sleepiness;

  } else if (mood.valence >= kHappyThreshold) {
    // 目を瞑った上向きの弧。この顔は口を持たないので笑いはここで表す。
    openTarget = 0.03f;
    archTarget = 1.0f;

  } else if (posture.inverted) {
    // 逆さにされて困っている
    browTarget = kInvertedBrow;
  }

  // --- 目標値へ補間する ---
  //
  // 閾値をまたいだ瞬間に顔が飛ばないようにする。
  approach(eyeOpen_, openTarget, kExpressionTauSec, dt);
  approach(eyeArch_, archTarget, kExpressionTauSec, dt);
  approach(browAngle_, browTarget, kExpressionTauSec, dt);

  // 覚醒度を瞳の大きさに出す。つつかれた驚きや興奮が目に表れる。
  const float irisTarget =
      kIrisScaleBase + clampf(mood.arousal, 0.0f, 1.0f) * kIrisScaleGain;
  approach(irisScale_, irisTarget, kExpressionTauSec, dt);

  // --- 目玉を可動域に収める ---
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
  //
  // すでに閉じている表情のときは、まばたきを重ねても意味がないので掛けない。
  const float blink = blinkOpenness(tMs);
  f.eyeOpen = (eyeOpen_ > 0.5f) ? eyeOpen_ * blink : eyeOpen_;
  f.eyeArch = eyeArch_;
  f.browAngle = browAngle_;
  f.irisScale = irisScale_;

  return f;
}

}  // namespace pet
