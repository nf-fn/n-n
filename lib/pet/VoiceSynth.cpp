#include "VoiceSynth.h"

#include <cmath>

namespace pet {
namespace {

constexpr float kTwoPi = 6.28318530718f;

// 正弦に倍音を混ぜた 1 周期分。純粋な正弦より芯が出て、
// 矩形波よりずっと柔らかい。合計が 1 を超えないよう正規化する。
inline float wave(float phase) {
  const float s1 = std::sin(phase);
  const float s2 = std::sin(phase * 2.0f);
  const float s3 = std::sin(phase * 3.0f);
  return (s1 + 0.26f * s2 + 0.09f * s3) / 1.35f;
}

inline float clamp01(float v) {
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

}  // namespace

size_t VoiceSynth::render(const VoiceCue &cue, int16_t *out,
                          size_t capacity) const {
  if (out == nullptr || capacity == 0 || cue.empty()) {
    return 0;
  }

  const float dt = 1.0f / static_cast<float>(kSampleRate);

  size_t written = 0;
  float phase = 0.0f;      // 音符をまたいで連続させる
  float vibPhase = 0.0f;
  float prevFreq = 0.0f;   // 0 は「前が無音」

  for (int n = 0; n < cue.count && written < capacity; ++n) {
    const VoiceNote &note = cue.notes[n];
    const size_t noteSamples =
        static_cast<size_t>(note.durMs) * kSampleRate / 1000u;
    if (noteSamples == 0) {
      continue;
    }

    // 無音の音符。長さぶん 0 を書き、次はポルタメントしない。
    if (note.freqHz == 0) {
      for (size_t i = 0; i < noteSamples && written < capacity; ++i) {
        out[written++] = 0;
      }
      prevFreq = 0.0f;
      continue;
    }

    const float target = static_cast<float>(note.freqHz);

    // 繋ぎの長さ。短い音符では音符自身の長さに合わせて縮める。
    size_t glideSamples = 0;
    if (prevFreq > 0.0f) {
      const size_t want = static_cast<size_t>(kGlideSec * kSampleRate);
      glideSamples = want < noteSamples / 2 ? want : noteSamples / 2;
    }

    const size_t attack =
        static_cast<size_t>(kAttackSec * kSampleRate) < noteSamples / 4
            ? static_cast<size_t>(kAttackSec * kSampleRate)
            : noteSamples / 4;
    const size_t release =
        static_cast<size_t>(kReleaseSec * kSampleRate) < noteSamples / 3
            ? static_cast<size_t>(kReleaseSec * kSampleRate)
            : noteSamples / 3;

    const bool isFirst = (n == 0) || (prevFreq == 0.0f);
    const bool isLast = (n == cue.count - 1);

    for (size_t i = 0; i < noteSamples && written < capacity; ++i) {
      // --- ピッチ ---
      float freq = target;
      if (i < glideSamples) {
        const float t = static_cast<float>(i) / glideSamples;
        freq = prevFreq + (target - prevFreq) * t;
      }

      vibPhase += kTwoPi * kVibratoHz * dt;
      if (vibPhase > kTwoPi) {
        vibPhase -= kTwoPi;
      }
      freq *= 1.0f + kVibratoDepth * std::sin(vibPhase);

      // --- 包絡 ---
      //
      // 音符の切れ目では繋がっているので傾きを付けない。
      // 付けると声の途中で音量が波打って聞こえる。
      float env = 1.0f;
      if (isFirst && attack > 0 && i < attack) {
        env = static_cast<float>(i) / attack;
      }
      if (isLast && release > 0 && i >= noteSamples - release) {
        const float t = static_cast<float>(noteSamples - i) / release;
        env = env < t ? env : t;
      }

      phase += kTwoPi * freq * dt;
      if (phase > kTwoPi) {
        phase -= kTwoPi;
      }

      const float v = wave(phase) * clamp01(env) * kAmplitude;
      out[written++] = static_cast<int16_t>(v * 32767.0f);
    }

    prevFreq = target;
  }

  return written;
}

}  // namespace pet
