// IMU の生サンプルを解釈する層。
//
// 出すもの:
//   Posture      … 連続量としての姿勢 (傾き、上下)
//   MotionEvent  … 瞬間的な出来事 (つつき、持ち上げ)
//   Activity     … 継続している状態 (静止、撫で、運搬、振り)
//
// 閾値はすべて test/fixtures/*.csv の実測から決めている。根拠は .cpp に残す。
#pragma once

#include "Types.h"

namespace pet {

class MotionAnalyzer {
 public:
  // 重力方向を平滑化する時定数 [秒]。
  //
  // 大きくすると振動に強くなるが傾けたときの追従が鈍る。
  // 0.15 秒は「1 秒間 3g で振り回しても姿勢が壊れず、
  // 0.5 秒で傾きに追従する」を満たす値 (テストで固定している)。
  static constexpr float kGravityTauSec = 0.15f;

  // 画面が上/下を向いていると判定する upZ の閾値
  static constexpr float kFaceThreshold = 0.7f;

  // 上下逆さと判定する upY の閾値
  static constexpr float kInvertedThreshold = -0.5f;

  // 特徴量の平滑化の時定数 [秒]。短いと分類が細かく揺れ、長いと反応が遅れる。
  static constexpr float kFeatureTauSec = 0.30f;

  void update(const ImuSample &sample);

  const Posture &posture() const { return posture_; }

  // この更新で起きた出来事。起きていなければ None。
  MotionEvent event() const { return event_; }

  // 現在の継続状態。
  Activity activity() const { return activity_; }

  // 分類に使っている特徴量。閾値の調整とデバッグ用。
  float linearAccel() const { return linEma_; }
  float angularRate() const { return gyroEma_; }
  float upwardAccel() const { return upwardAccel_; }

 private:
  void recomputePosture();
  void updateFeatures(const ImuSample &sample, float dt);
  void detectTap(const ImuSample &sample);
  void detectLift();
  void updateActivity(float dt);

  // 平滑化した「世界の上」方向 (機体座標)。正規化前の生の平均値。
  float upX_ = 0.0f;
  float upY_ = 0.0f;
  float upZ_ = 0.0f;

  // 重力を引いた線形加速度の大きさと、角速度の大きさ。どちらも平滑化済み。
  float linEma_ = 0.0f;
  float gyroEma_ = 0.0f;

  // 前サンプルの加速度。差分 (ジャーク) がつつきの手がかりになる。
  float prevAx_ = 0.0f;
  float prevAy_ = 0.0f;
  float prevAz_ = 0.0f;

  // 重力方向に沿った線形加速度。持ち上げの判定に使う。
  float upwardAccel_ = 0.0f;

  bool initialized_ = false;
  uint32_t lastMs_ = 0;
  uint32_t nowMs_ = 0;

  // つつきの不応期。1 回の衝撃で何度も発火させない。
  uint32_t tapBlockedUntilMs_ = 0;
  // 持ち上げ直後はつつきを止める。置いたときの衝撃を拾わないため。
  uint32_t liftBlockedUntilMs_ = 0;
  // つつき直後は接触 (撫で/運搬) の判定を止める。余韻を撫でと取らないため。
  uint32_t contactBlockedUntilMs_ = 0;

  Activity activity_ = Activity::Quiet;
  Activity candidate_ = Activity::Quiet;
  float candidateHeldSec_ = 0.0f;

  MotionEvent event_ = MotionEvent::None;

  Posture posture_;
};

}  // namespace pet
