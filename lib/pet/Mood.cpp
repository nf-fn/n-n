#include "Mood.h"

namespace pet {
namespace {

// --- 基準値 ---
//
// 何もされなければ各値はここへ戻る。
constexpr float kArousalBaseline = 0.3f;

// --- 減衰の時定数 [秒] ---
//
// 速さの序列に意味がある。めまいはすぐ抜けるが、怒りは冷めにくい。
// これが「さっきまで乱暴にされていた」という記憶として働く。
constexpr float kArousalTau = 2.0f;
constexpr float kValenceTau = 6.0f;
constexpr float kDizzinessTau = 2.5f;
constexpr float kAngerTau = 10.0f;

// --- 刺激の量 ---

// つつかれたとき、持ち上げられたときの覚醒の跳ね
constexpr float kTapArousal = 0.45f;
constexpr float kLiftArousal = 0.60f;

// 目覚め。眠っているときに触られたら覚める。
constexpr float kTapWake = 0.45f;
constexpr float kLiftWake = 0.70f;

// --- 継続した状態が 1 秒あたりに与える量 ---

constexpr float kStrokeValenceRate = 0.70f;
constexpr float kStrokeAngerRate = -0.45f;   // 撫でると怒りが冷める
constexpr float kStrokeSleepRate = -0.20f;
constexpr float kStrokeArousalTarget = 0.5f;

constexpr float kShakeDizzyRate = 0.90f;
constexpr float kShakeAngerRate = 0.18f;     // 3 秒ほど振られて怒りに届く
constexpr float kShakeValenceRate = -0.30f;
constexpr float kShakeSleepRate = -1.5f;
constexpr float kShakeArousalTarget = 0.95f;

constexpr float kHeldSleepRate = -0.25f;
constexpr float kHeldArousalTarget = 0.55f;

// 放置。40 秒ほどでうとうとし始める。
constexpr float kQuietSleepRate = 0.025f;

// 覚醒度が目標へ寄る速さ [1/秒]
constexpr float kArousalPullRate = 2.0f;

float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// 目標値へ指数的に近づける
void decayToward(float &value, float target, float tau, float dt) {
  const float alpha = dt / (tau + dt);
  value += alpha * (target - value);
}

}  // namespace

void Mood::update(MotionEvent event, Activity activity, float dt) {
  if (dt <= 0.0f) {
    return;
  }

  // --- まず自然な減衰 ---
  //
  // 刺激より先に減衰させる。こうしないと、刺激を受けた最初のフレームで
  // その刺激自身が減衰され、入力が弱まってしまう。
  decayToward(state_.arousal, kArousalBaseline, kArousalTau, dt);
  decayToward(state_.valence, 0.0f, kValenceTau, dt);
  decayToward(state_.dizziness, 0.0f, kDizzinessTau, dt);
  decayToward(state_.anger, 0.0f, kAngerTau, dt);

  // --- 継続している状態 ---

  switch (activity) {
    case Activity::Stroke:
      state_.valence += kStrokeValenceRate * dt;
      state_.anger += kStrokeAngerRate * dt;
      state_.sleepiness += kStrokeSleepRate * dt;
      state_.arousal +=
          (kStrokeArousalTarget - state_.arousal) * kArousalPullRate * dt;
      break;

    case Activity::Shake:
      state_.dizziness += kShakeDizzyRate * dt;
      state_.anger += kShakeAngerRate * dt;
      state_.valence += kShakeValenceRate * dt;
      state_.sleepiness += kShakeSleepRate * dt;
      state_.arousal +=
          (kShakeArousalTarget - state_.arousal) * kArousalPullRate * dt;
      break;

    case Activity::Held:
      state_.sleepiness += kHeldSleepRate * dt;
      state_.arousal +=
          (kHeldArousalTarget - state_.arousal) * kArousalPullRate * dt;
      break;

    case Activity::Quiet:
      state_.sleepiness += kQuietSleepRate * dt;
      break;
  }

  // --- 瞬間的な出来事 ---

  switch (event) {
    case MotionEvent::Tap:
      state_.arousal += kTapArousal;
      state_.sleepiness -= kTapWake;
      break;

    case MotionEvent::Lift:
      state_.arousal += kLiftArousal;
      state_.sleepiness -= kLiftWake;
      break;

    case MotionEvent::None:
      break;
  }

  // --- 範囲に収める ---
  //
  // 連続量を積み上げるので、これが無いと際限なく溜まる。
  state_.arousal = clampf(state_.arousal, 0.0f, 1.0f);
  state_.valence = clampf(state_.valence, -1.0f, 1.0f);
  state_.dizziness = clampf(state_.dizziness, 0.0f, 1.0f);
  state_.anger = clampf(state_.anger, 0.0f, 1.0f);
  state_.sleepiness = clampf(state_.sleepiness, 0.0f, 1.0f);
}

}  // namespace pet
