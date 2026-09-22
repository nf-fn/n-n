// IMU の生サンプルを解釈する層。
//
// フェーズ 1 では姿勢 (Posture) のみを出す。
// フェーズ 2 で MotionEvent の検出を足す。
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

  void update(const ImuSample &sample);

  const Posture &posture() const { return posture_; }

 private:
  void recomputePosture();

  // 平滑化した「世界の上」方向 (機体座標)。正規化前の生の平均値。
  float upX_ = 0.0f;
  float upY_ = 0.0f;
  float upZ_ = 0.0f;

  bool initialized_ = false;
  uint32_t lastMs_ = 0;

  Posture posture_;
};

}  // namespace pet
