#include "MotionAnalyzer.h"

#include <cmath>

namespace pet {
namespace {

// タイムスタンプが飛んだり巻き戻ったりしたときに使う既定の刻み [ms]。
// センサは 100Hz で回す想定。
constexpr uint32_t kDefaultStepMs = 10;
constexpr uint32_t kMaxStepMs = 100;

// --- 閾値以外の定数 ---
//
// 大きさの閾値は Thresholds に出してある (キャリブレーションで差し替わる)。
// ここに残すのは時間まわりの定数で、触り方の癖では変わらない性質のもの。

// 分類を切り替えるのに必要な継続時間 [秒]。
constexpr float kActivityHoldSec = 0.40f;

// 撫でに入るときだけは長く待つ。撫では本来ずっと続く動作なので、
// 短い窓で成立させる必要がない。手に持っているあいだに角速度が一瞬下がる窓や、
// つつきの余韻を撫でと取り違えるのを防ぐ。
//
// 1.0 秒まで縮めると walk.csv を撫でと誤判定した。フィクスチャは 1 本しか
// 無いので、失敗する値に対して 0.3 秒の余裕を残してある。
constexpr float kStrokeHoldSec = 1.30f;

// つつきの直後は接触の判定を止める。衝撃の余韻が撫でに見えるため。
constexpr uint32_t kContactBlockAfterTapMs = 800;

// 1 回の衝撃で何度もつつきを発火させないための不応期。
constexpr uint32_t kTapRefractoryMs = 250;

// 持ち上げは「上向きの加速が続く」こと。つつきの衝撃は 1 サンプルで終わるので、
// 短い時定数で平滑化してから見れば区別できる。
// 1g の衝撃が 1 サンプル入っても、この平滑化後は 0.09 程度にしかならない。
constexpr float kUpwardTauSec = 0.10f;

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
  // detectTap が接触判定を止めることがあるので、Activity より先に走らせる。
  // 持ち上げは Activity の遷移も手がかりにするため、その後に見る。
  updateActivity(dt);
  detectLift();

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
  const float dx = sample.ax - prevAx_;
  const float dy = sample.ay - prevAy_;
  const float dz = sample.az - prevAz_;
  jerk_ = std::sqrt(dx * dx + dy * dy + dz * dz);

  if (nowMs_ < tapBlockedUntilMs_ || nowMs_ < liftBlockedUntilMs_) {
    return;
  }

  if (jerk_ < thresholds_.tapJerk) {
    return;
  }

  // 鋭い衝撃だけでは足りない。歩行や振りの最中にも大きな差分は出る。
  // つつきは「静かな背景に突然入る」ことが特徴なので、背景の静けさを見る。
  if (linEma_ > thresholds_.tapMaxBackground) {
    return;
  }

  event_ = MotionEvent::Tap;
  tapBlockedUntilMs_ = nowMs_ + kTapRefractoryMs;
  contactBlockedUntilMs_ = nowMs_ + kContactBlockAfterTapMs;
}

void MotionAnalyzer::detectLift() {
  const bool leftRest = justLeftRest_;
  justLeftRest_ = false;

  if (event_ != MotionEvent::None) {
    return;  // 同じ更新でつつきが出ていれば、そちらを優先する
  }
  if (nowMs_ < liftBlockedUntilMs_) {
    return;
  }

  // 持ち上げは 2 つの経路で拾う。速い持ち上げも遅い持ち上げも取るため。
  //
  // 1. 上向きの加速が強い … 勢いよく持ち上げた場合。すぐ発火する。
  //
  // 2. 静止から「扱われている」状態へ遷移した … ゆっくり持ち上げた場合。
  //    重力の推定は 0.15 秒で追従するので、ゆっくり持ち上げると上向きの
  //    加速度が重力側に吸収されて経路 1 では拾えない。状態の遷移で拾う。
  //    撫でへの遷移は持ち上げではないので除く。
  const bool liftedFast = upwardAccel_ >= thresholds_.liftUpwardAccel;
  if (!liftedFast && !leftRest) {
    return;
  }

  event_ = MotionEvent::Lift;
  liftBlockedUntilMs_ = nowMs_ + kLiftRefractoryMs;
}

void MotionAnalyzer::updateActivity(float dt) {
  // まず今この瞬間の見立てを作る
  Activity now = Activity::Quiet;
  if (linEma_ >= thresholds_.shakeMinLin) {
    now = Activity::Shake;
  } else if (nowMs_ < contactBlockedUntilMs_) {
    // つつきの直後。余韻が続いているだけで、触られ続けてはいない。
    now = Activity::Quiet;
  } else if (linEma_ >= thresholds_.contactMinLin && linEma_ < thresholds_.contactMaxLin) {
    // 撫でと手持ちは線形加速度が重なる。角速度で分ける。
    // 机の上の本体を撫でてもほとんど回転しないが、手に持つと回る。
    now = (gyroEma_ < thresholds_.strokeMaxGyro) ? Activity::Stroke : Activity::Held;
  } else if (linEma_ > thresholds_.quietLin) {
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
    // 静止から「手で扱われている」状態に移ったら持ち上げの手がかりになる。
    // 撫では机の上でも起きるので、持ち上げとは見なさない。
    if (activity_ == Activity::Quiet && (candidate_ == Activity::Held ||
                                         candidate_ == Activity::Shake)) {
      justLeftRest_ = true;
    }
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
