#include "Calibration.h"

#include <algorithm>

namespace pet {
namespace {

// 導出した値を必ずこの範囲に収める。壊れた測定が焼き付いても、
// 検出が完全に死なないようにするため。
constexpr float kContactMinLinLo = 0.015f;
constexpr float kContactMinLinHi = 0.15f;
constexpr float kStrokeGyroLo = 8.0f;
constexpr float kStrokeGyroHi = 40.0f;
constexpr float kShakeMinLinLo = 0.15f;
constexpr float kShakeMinLinHi = 0.90f;
constexpr float kTapJerkLo = 0.25f;
constexpr float kTapJerkHi = 1.60f;
constexpr float kTapBackgroundLo = 0.03f;
constexpr float kTapBackgroundHi = 0.25f;
constexpr float kLiftUpwardLo = 0.15f;
constexpr float kLiftUpwardHi = 0.90f;

float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// 立ち上がりを捨てるため、最初のぶんは percentile から除く。
constexpr int kWarmupSamples = 60;

float percentile(const float *values, int count, float p) {
  if (count <= kWarmupSamples) {
    return 0.0f;
  }
  const int begin = kWarmupSamples;
  const int n = count - begin;

  // 小さいので素直にコピーして並べ替える
  static float scratch[Calibrator::kMaxSamples];
  std::copy(values + begin, values + count, scratch);
  std::sort(scratch, scratch + n);

  int idx = static_cast<int>(p * n);
  if (idx >= n) {
    idx = n - 1;
  }
  if (idx < 0) {
    idx = 0;
  }
  return scratch[idx];
}

}  // namespace

Thresholds resolve(const CalibrationSet &set) {
  Thresholds t;  // 既定値から始める

  const MotionCalib &tap = set.at(CalibTarget::Tap);
  if (tap.hasData && tap.enabled) {
    t.tapJerk = tap.a;
    t.tapMaxBackground = tap.b;
  }

  const MotionCalib &stroke = set.at(CalibTarget::Stroke);
  if (stroke.hasData && stroke.enabled) {
    t.contactMinLin = stroke.a;
    t.strokeMaxGyro = stroke.b;
  }

  const MotionCalib &shake = set.at(CalibTarget::Shake);
  if (shake.hasData && shake.enabled) {
    t.shakeMinLin = shake.a;
  }

  const MotionCalib &lift = set.at(CalibTarget::Lift);
  if (lift.hasData && lift.enabled) {
    t.liftUpwardAccel = lift.a;
  }

  return t;
}

void Calibrator::begin(CalibTarget target) {
  target_ = target;
  count_ = 0;
  maxJerk_ = 0.0f;
  maxUpward_ = 0.0f;
  analyzer_ = MotionAnalyzer{};
}

void Calibrator::feed(const ImuSample &sample) {
  analyzer_.update(sample);

  if (analyzer_.jerk() > maxJerk_) {
    maxJerk_ = analyzer_.jerk();
  }
  if (analyzer_.upwardAccel() > maxUpward_) {
    maxUpward_ = analyzer_.upwardAccel();
  }

  if (count_ < kMaxSamples) {
    lin_[count_] = analyzer_.linearAccel();
    gyro_[count_] = analyzer_.angularRate();
    ++count_;
  }
}

Calibrator::Result Calibrator::finish() const {
  if (count_ <= kWarmupSamples) {
    Result r;
    r.reason = "短すぎ";
    return r;
  }

  switch (target_) {
    case CalibTarget::Tap: return finishTap();
    case CalibTarget::Stroke: return finishStroke();
    case CalibTarget::Shake: return finishShake();
    case CalibTarget::Lift: return finishLift();
  }
  return Result{};
}

Calibrator::Result Calibrator::finishTap() const {
  Result r;

  // つつきは「静かな背景に鋭い衝撃が入る」動作。両方が揃って初めて成立する。
  if (maxJerk_ < 0.35f) {
    r.reason = "衝撃が弱い";
    return r;
  }
  const float background = percentile(lin_, count_, 0.50f);
  if (background > 0.20f) {
    r.reason = "動かしすぎ";
    return r;
  }

  // 一番強く叩いたときの半分弱を閾値にする。弱めのつつきも拾うため。
  r.calib.a = clampf(maxJerk_ * 0.45f, kTapJerkLo, kTapJerkHi);
  // 背景の上限は、実際の背景より少し上に置く。
  r.calib.b = clampf(percentile(lin_, count_, 0.95f) * 1.4f + 0.02f,
                     kTapBackgroundLo, kTapBackgroundHi);
  r.calib.hasData = true;
  r.ok = true;
  return r;
}

Calibrator::Result Calibrator::finishStroke() const {
  Result r;

  const float linLo = percentile(lin_, count_, 0.10f);
  const float linMid = percentile(lin_, count_, 0.50f);
  const float gyroHi = percentile(gyro_, count_, 0.95f);

  // 触っていなければ撫でではない
  if (linMid < 0.02f) {
    r.reason = "触れていない";
    return r;
  }
  // 大きく動かしていたら撫でではなく手持ちか振り
  if (linMid > 0.35f) {
    r.reason = "動かしすぎ";
    return r;
  }
  // 本体ごと回していたら、手持ちと区別が付かなくなる
  if (gyroHi > 45.0f) {
    r.reason = "本体が動きすぎ";
    return r;
  }

  r.calib.a = clampf(linLo * 0.6f, kContactMinLinLo, kContactMinLinHi);
  // 撫でたときの角速度の上に余裕を置く。ここが手持ちとの境界になる。
  r.calib.b = clampf(gyroHi * 1.35f + 2.0f, kStrokeGyroLo, kStrokeGyroHi);
  r.calib.hasData = true;
  r.ok = true;
  return r;
}

Calibrator::Result Calibrator::finishShake() const {
  Result r;

  const float linLo = percentile(lin_, count_, 0.10f);
  if (linLo < 0.20f) {
    r.reason = "振りが弱い";
    return r;
  }

  r.calib.a = clampf(linLo * 0.75f, kShakeMinLinLo, kShakeMinLinHi);
  r.calib.hasData = true;
  r.ok = true;
  return r;
}

Calibrator::Result Calibrator::finishLift() const {
  Result r;

  if (maxUpward_ < 0.20f) {
    r.reason = "持ち上げが弱い";
    return r;
  }

  // 一番勢いよく持ち上げたときの半分強。ゆっくりした持ち上げは
  // 状態遷移の経路で拾うので、ここは速い持ち上げ用の値になる。
  r.calib.a = clampf(maxUpward_ * 0.55f, kLiftUpwardLo, kLiftUpwardHi);
  r.calib.hasData = true;
  r.ok = true;
  return r;
}

}  // namespace pet
