#include "MotionAnalyzer.h"

#include <cmath>

namespace pet {
namespace {

// タイムスタンプが飛んだり巻き戻ったりしたときに使う既定の刻み [ms]。
// センサは 100Hz で回す想定。
constexpr uint32_t kDefaultStepMs = 10;
constexpr uint32_t kMaxStepMs = 100;

// --- 分類の閾値 ---
//
// すべて test/fixtures/*.csv の実測から決めた。平滑化後 (tau 0.30s) の
// 線形加速度 [g] と角速度 [deg/s] の p10〜p90 は次のとおりだった:
//
//   動作     線形加速度      角速度
//   idle     0.001-0.002     0.5
//   stroke   0.069-0.089     9.1-14.9
//   walk     0.091-0.225    27.6-55.2
//   shake    0.556-0.813    59.8-89.0
//   lift     0.067-0.476    15.6-118.7
//   tap      0.008-0.072     1.5-15.1  (背景は静かで、衝撃だけが鋭い)

// 静止と見なす上限。idle の 0.002 と stroke の 0.069 の間。
constexpr float kQuietLin = 0.02f;

// 撫で・運搬と見なす線形加速度の下限。tap の裾 (0.072) を避けつつ
// stroke の下限 (0.069) は拾いたいので、持続時間の条件と併用する。
constexpr float kContactMinLin = 0.045f;

// 撫で・運搬の上限。walk の上限 0.225 より上、shake の下限 0.556 より下。
constexpr float kContactMaxLin = 0.30f;

// 撫でと運搬を分ける角速度。ここが最も重要な境界。
// 大きさでは重なる撫で (9-15) と歩行 (28-55) を、これだけで分離できる。
constexpr float kStrokeMaxGyro = 20.0f;

// 振りと見なす線形加速度の下限。walk の上限 0.225 と shake の下限 0.556 の間。
constexpr float kShakeMinLin = 0.30f;

// 分類を切り替えるのに必要な継続時間 [秒]。
constexpr float kActivityHoldSec = 0.40f;

// 撫でに入るときだけは長く待つ。撫では本来ずっと続く動作なので、
// 短い窓で成立させる必要がない。歩行中に角速度が一瞬下がる窓や、
// つつきの余韻を撫でと取り違えるのを防ぐ。
constexpr float kStrokeHoldSec = 1.50f;

// つつきの直後は接触の判定を止める。衝撃の余韻が撫でに見えるため。
constexpr uint32_t kContactBlockAfterTapMs = 800;

// --- つつきの閾値 ---

// 1 サンプル間の加速度変化 [g]。
// tap の最大 2.03 に対し、shake 0.55 / stroke 0.28 / walk 0.95。
constexpr float kTapJerk = 0.80f;

// つつきと認めるための背景の静けさ。
// 衝撃時の線形加速度の中央値は tap 0.071 / lift 0.109 / walk 0.222 だった。
constexpr float kTapMaxBackground = 0.10f;

constexpr uint32_t kTapRefractoryMs = 250;

// --- 持ち上げの閾値 ---

// 持ち上げは「上向きの加速が続く」こと。つつきの衝撃は 1 サンプルで終わるので、
// 短い時定数で平滑化してから見れば区別できる。
// 1g の衝撃が 1 サンプル入っても、この平滑化後は 0.09 程度にしかならない。
constexpr float kUpwardTauSec = 0.10f;

// 平滑化後の上向き加速度の最大値は、実測で次のとおりだった:
//   lift 0.59 / walk 0.28 / shake 0.19 / tap 0.03 / stroke 0.02
// 0.35 に置けば持ち上げだけが残る。歩行中の揺れとは大きさで分離できる。
constexpr float kLiftUpwardAccel = 0.35f;

constexpr uint32_t kLiftRefractoryMs = 800;

}  // namespace

void MotionAnalyzer::update(const ImuSample &sample) {
  event_ = MotionEvent::None;

  if (!initialized_) {
    // 最初のサンプルは平滑化せずそのまま採る。
    // 起動直後に顔が中央から流れてくるのを避ける。
    upX_ = sample.ax;
    upY_ = sample.ay;
    upZ_ = sample.az;
    prevAx_ = sample.ax;
    prevAy_ = sample.ay;
    prevAz_ = sample.az;
    initialized_ = true;
    lastMs_ = sample.tMs;
    nowMs_ = sample.tMs;
    recomputePosture();
    return;
  }

  uint32_t stepMs = sample.tMs - lastMs_;
  if (stepMs == 0 || stepMs > kMaxStepMs) {
    // 巻き戻り (uint32 の減算で巨大値になる) と長すぎる欠測をまとめて弾く
    stepMs = kDefaultStepMs;
  }
  lastMs_ = sample.tMs;
  nowMs_ += stepMs;  // 巻き戻りに影響されない単調な時計

  const float dt = static_cast<float>(stepMs) / 1000.0f;
  const float alpha = dt / (kGravityTauSec + dt);

  upX_ += alpha * (sample.ax - upX_);
  upY_ += alpha * (sample.ay - upY_);
  upZ_ += alpha * (sample.az - upZ_);

  recomputePosture();

  updateFeatures(sample, dt);
  detectTap(sample);
  detectLift();
  updateActivity(dt);

  prevAx_ = sample.ax;
  prevAy_ = sample.ay;
  prevAz_ = sample.az;
}

void MotionAnalyzer::updateFeatures(const ImuSample &sample, float dt) {
  // 重力を引いた残りが、外から加えられた加速度。
  const float lx = sample.ax - upX_;
  const float ly = sample.ay - upY_;
  const float lz = sample.az - upZ_;
  const float lin = std::sqrt(lx * lx + ly * ly + lz * lz);

  const float gyro = std::sqrt(sample.gx * sample.gx + sample.gy * sample.gy +
                               sample.gz * sample.gz);

  const float alpha = dt / (kFeatureTauSec + dt);
  linEma_ += alpha * (lin - linEma_);
  gyroEma_ += alpha * (gyro - gyroEma_);

  // 重力方向への射影。持ち上げると正、下ろすと負になる。
  // posture_ の up は正規化済み。
  const float upward =
      lx * posture_.upX + ly * posture_.upY + lz * posture_.upZ;
  const float upAlpha = dt / (kUpwardTauSec + dt);
  upwardAccel_ += upAlpha * (upward - upwardAccel_);
}

void MotionAnalyzer::detectTap(const ImuSample &sample) {
  if (nowMs_ < tapBlockedUntilMs_ || nowMs_ < liftBlockedUntilMs_) {
    return;
  }

  const float dx = sample.ax - prevAx_;
  const float dy = sample.ay - prevAy_;
  const float dz = sample.az - prevAz_;
  const float jerk = std::sqrt(dx * dx + dy * dy + dz * dz);

  if (jerk < kTapJerk) {
    return;
  }

  // 鋭い衝撃だけでは足りない。歩行や振りの最中にも大きな差分は出る。
  // つつきは「静かな背景に突然入る」ことが特徴なので、背景の静けさを見る。
  if (linEma_ > kTapMaxBackground) {
    return;
  }

  event_ = MotionEvent::Tap;
  tapBlockedUntilMs_ = nowMs_ + kTapRefractoryMs;
  contactBlockedUntilMs_ = nowMs_ + kContactBlockAfterTapMs;
}

void MotionAnalyzer::detectLift() {
  if (event_ != MotionEvent::None) {
    return;  // 同じ更新でつつきが出ていれば、そちらを優先する
  }
  if (nowMs_ < liftBlockedUntilMs_) {
    return;
  }

  // 当初は「直近で静止していたこと」も条件にしていたが、外した。
  // 持ち上げて置いてをくり返すと机の上でも揺れが収まりきらず、
  // 実測では静止フレームが 998 中 88 しかなく、9 回の持ち上げのうち
  // 1 回しか拾えなかった。上向き加速度の大きさだけで歩行と分離できるため、
  // 静止の条件は不要だった。
  if (upwardAccel_ < kLiftUpwardAccel) {
    return;
  }

  event_ = MotionEvent::Lift;
  liftBlockedUntilMs_ = nowMs_ + kLiftRefractoryMs;
}

void MotionAnalyzer::updateActivity(float dt) {
  // まず今この瞬間の見立てを作る
  Activity now = Activity::Quiet;
  if (linEma_ >= kShakeMinLin) {
    now = Activity::Shake;
  } else if (nowMs_ < contactBlockedUntilMs_) {
    // つつきの直後。余韻が続いているだけで、触られ続けてはいない。
    now = Activity::Quiet;
  } else if (linEma_ >= kContactMinLin && linEma_ < kContactMaxLin) {
    // 撫でと運搬は線形加速度が重なる。角速度で分ける。
    // 机の上の本体を撫でてもほとんど回転しないが、運ばれると回る。
    now = (gyroEma_ < kStrokeMaxGyro) ? Activity::Stroke : Activity::Carried;
  } else if (linEma_ > kQuietLin) {
    // 静止と接触の間の帯。どちらとも言えないので直前の判断を保つ。
    now = candidate_;
  }

  // ちらつきを抑えるため、一定時間続いて初めて切り替える。
  if (now == candidate_) {
    candidateHeldSec_ += dt;
  } else {
    candidate_ = now;
    candidateHeldSec_ = 0.0f;
  }

  // 撫でに入るときだけ長く待つ。歩行中に角速度が一瞬下がる窓や、
  // 衝撃の余韻で撫でに化けるのを防ぐ。
  const float required = (candidate_ == Activity::Stroke) ? kStrokeHoldSec
                                                          : kActivityHoldSec;

  if (candidate_ != activity_ && candidateHeldSec_ >= required) {
    activity_ = candidate_;
  }
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
