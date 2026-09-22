// キャリブレーション。
//
// 既定の閾値は 1 人が 1 回録音しただけの値で、触り方の癖には合っていない。
// 各動作を実機で録り直し、その人の値に合わせるための層。
//
// 「測ったか (hasData)」と「使うか (enabled)」を分けて持つ。測ったまま
// 無効にできるので、気に入らなければ測り直さずに既定値へ戻せる。
//
// 導出した値は必ず妥当性を検査し、範囲にも収める。壊れた値が焼き付くと
// 以後まともに反応しなくなるため。
#pragma once

#include "MotionAnalyzer.h"
#include "Thresholds.h"
#include "Types.h"

namespace pet {

enum class CalibTarget {
  Tap,
  Stroke,
  Shake,
  Lift,
};
constexpr int kCalibTargetCount = 4;

// 動作 1 つぶんの測定結果。値の意味は動作ごとに違う。
//   Tap    a = つつきのジャーク閾値      b = 背景の静けさの上限
//   Stroke a = 接触と見なす下限          b = 撫でと手持ちを分ける角速度
//   Shake  a = 振りと見なす下限          b = 未使用
//   Lift   a = 持ち上げの上向き加速度    b = 未使用
struct MotionCalib {
  bool hasData = false;
  bool enabled = false;
  float a = 0.0f;
  float b = 0.0f;
};

struct CalibrationSet {
  MotionCalib motions[kCalibTargetCount];

  MotionCalib &at(CalibTarget t) {
    return motions[static_cast<int>(t)];
  }
  const MotionCalib &at(CalibTarget t) const {
    return motions[static_cast<int>(t)];
  }
};

// 測定結果を閾値に反映する。有効になっているものだけが既定値を上書きする。
Thresholds resolve(const CalibrationSet &set);

// 測定中にサンプルを溜め、終了時に閾値の候補を出す。
//
// 特徴量は MotionAnalyzer を内部で回して読む。検出側と同じ計算を使うので、
// 「測った値」と「判定に使う値」がずれない。
class Calibrator {
 public:
  // 1 回の測定で溜めるサンプルの上限。100Hz で 20 秒ぶん。
  static constexpr int kMaxSamples = 2048;

  struct Result {
    bool ok = false;
    MotionCalib calib;
    const char *reason = "";  // ok が false のときの理由
  };

  void begin(CalibTarget target);
  void feed(const ImuSample &sample);
  Result finish() const;

  int sampleCount() const { return count_; }
  CalibTarget target() const { return target_; }

 private:
  Result finishTap() const;
  Result finishStroke() const;
  Result finishShake() const;
  Result finishLift() const;

  CalibTarget target_ = CalibTarget::Tap;
  int count_ = 0;

  // 溜めた特徴量。動作によって使うものが違う。
  float lin_[kMaxSamples];
  float gyro_[kMaxSamples];

  float maxJerk_ = 0.0f;
  float maxUpward_ = 0.0f;

  // 特徴量を出すための解析器。既定の閾値のまま使う。
  // ここで読むのは判定結果ではなく生の特徴量なので、閾値には影響されない。
  MotionAnalyzer analyzer_;
};

}  // namespace pet
