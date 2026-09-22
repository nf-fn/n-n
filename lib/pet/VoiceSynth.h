// 音符の並びを PCM の波形に変える層。
//
// tone() の矩形波はブザーに聞こえる。生き物の声に近づけるために、
// 次の 4 つを入れている:
//
//   1. 正弦波に倍音を少し混ぜる … 純粋な正弦は柔らかすぎて芯がない
//   2. 音符の変わり目を滑らかに繋ぐ (ポルタメント) … 段差が機械的に聞こえる
//   3. 立ち上がりと立ち下がりに傾きを付ける … 急に切れるとプチッと鳴る
//   4. わずかなビブラート … 一定のピッチは無機質に聞こえる
//
// 位相は音符をまたいで連続させる。途切れさせるとそこで必ず雑音が出る。
#pragma once

#include <cstddef>
#include <cstdint>

#include "Voice.h"

namespace pet {

class VoiceSynth {
 public:
  static constexpr uint32_t kSampleRate = 16000;

  // 音符 6 つ × 最長 260ms に余裕を足した長さ
  static constexpr size_t kMaxSamples = 26000;

  // 前後の音符を繋ぐ時間 [秒]。これより短い音符では比率で縮める。
  static constexpr float kGlideSec = 0.055f;

  // 立ち上がりと立ち下がり [秒]
  static constexpr float kAttackSec = 0.012f;
  static constexpr float kReleaseSec = 0.030f;

  // ビブラート
  static constexpr float kVibratoHz = 5.5f;
  static constexpr float kVibratoDepth = 0.014f;  // ±1.4%

  // 振幅。倍音を足すぶん余裕を残す。
  static constexpr float kAmplitude = 0.62f;

  // cue を out に描き、書いたサンプル数を返す。
  // capacity が足りなければ書ける範囲で打ち切る。
  size_t render(const VoiceCue &cue, int16_t *out, size_t capacity) const;
};

}  // namespace pet
