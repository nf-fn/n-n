// 姿勢と気分から顔の描画パラメータを組み立てる層。
//
// この顔は口を持たないため、感情は目と眉だけで表す。
//   笑い = 目を瞑った上向きの弧 / 怒り = 眉の内側を下げる
//   眠気 = 深い下向きの弧       / めまい = 瞳が回る
//
// 気分が競合したときは強い感情が勝つ。
// めまい > 怒り > 眠気 > 笑い の順で、閾値を超えたものが顔を占領する。
// 複数を混ぜると何を感じているのか読み取れなくなるため。
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

  // 感情が顔を占領する閾値
  static constexpr float kDizzyThreshold = 0.45f;
  static constexpr float kAngerThreshold = 0.45f;
  static constexpr float kSleepyThreshold = 0.60f;
  static constexpr float kHappyThreshold = 0.45f;

  // 表情が切り替わるときの補間の時定数 [秒]。
  // 無いと閾値をまたいだ瞬間に顔が飛ぶ。
  static constexpr float kExpressionTauSec = 0.15f;

  // 覚醒度 0..1 を瞳の倍率に写す。覚醒度の基準値 0.3 でほぼ 1.0 になる。
  static constexpr float kIrisScaleBase = 0.88f;
  static constexpr float kIrisScaleGain = 0.40f;

  // 姿勢・気分・現在時刻から 1 フレーム分のパラメータを作る。
  // まばたきと補間の進行があるため const ではない。
  FaceParams compose(const Posture &posture, const MoodState &mood,
                     uint32_t tMs);

 private:
  float blinkOpenness(uint32_t tMs);

  // 決定的な擬似乱数。まばたきの間隔をばらつかせるためだけに使う。
  // テストで再現できるよう、固定の種から始める。
  uint32_t rand32();
  uint32_t rngState_ = 0x9e3779b9u;

  bool blinkScheduled_ = false;
  uint32_t blinkStartMs_ = 0;

  // 補間中の表情。目標値へ向かって少しずつ動く。
  float eyeOpen_ = 1.0f;
  float eyeArch_ = 0.0f;
  float browAngle_ = 0.0f;
  float irisScale_ = 1.0f;

  bool timeKnown_ = false;
  uint32_t lastMs_ = 0;
};

}  // namespace pet
