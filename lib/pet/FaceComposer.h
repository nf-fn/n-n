// 姿勢から顔の描画パラメータを組み立てる層。
//
// フェーズ 1 の責務は「目玉を下り坂へ転がす」「顔を少し傾ける」
// 「まばたきする」の 3 つ。気分による表情はフェーズ 3 で足す。
#pragma once

#include "Types.h"

namespace pet {

class FaceComposer {
 public:
  // 目玉の可動量。下り坂ベクトル (長さ 0..1) にこれを掛ける。
  // 1.0 で、画面を垂直にしたとき目玉が可動域の端まで落ちる。
  static constexpr float kEyeTravel = 1.0f;

  // 顔が世界の水平へ戻ろうとする割合。
  // 1.0 にすると水準器のようになって生き物に見えないため、
  // 戻しきらずに傾きを残す。
  static constexpr float kTiltFollow = 0.3f;

  // まばたき 1 回の長さ [ms]
  static constexpr uint32_t kBlinkDurationMs = 180;

  // まばたきの間隔 [ms]。この範囲で毎回ばらつかせる。
  static constexpr uint32_t kBlinkIntervalMinMs = 2000;
  static constexpr uint32_t kBlinkIntervalMaxMs = 6000;

  // 姿勢と現在時刻から 1 フレーム分のパラメータを作る。
  // まばたきの進行があるため const ではない。
  FaceParams compose(const Posture &posture, uint32_t tMs);

 private:
  float blinkOpenness(uint32_t tMs);

  // 決定的な擬似乱数。まばたきの間隔をばらつかせるためだけに使う。
  // テストで再現できるよう、固定の種から始める。
  uint32_t rand32();
  uint32_t rngState_ = 0x9e3779b9u;

  bool blinkScheduled_ = false;
  uint32_t blinkStartMs_ = 0;
};

}  // namespace pet
