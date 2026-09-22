// 鳴き声を組み立てる層。
//
// この顔は口を持たず、言葉も話さない。ピッチと長さだけで感情を表す。
// 鳴くかどうかの判断と音符の並びを決めるだけで、音は鳴らさない。
// 実際の再生は I/O 側 (VoiceOutput) の責務。
#pragma once

#include "Types.h"

namespace pet {

// メンバ初期化子は付けない。付けると集約でなくなり、
// C++11 でビルドされる実機側で {1400, 55} の形が使えなくなる。
struct VoiceNote {
  uint16_t freqHz;  // 0 は無音
  uint16_t durMs;
};

constexpr int kMaxVoiceNotes = 6;

struct VoiceCue {
  VoiceNote notes[kMaxVoiceNotes] = {};
  int count = 0;

  bool empty() const { return count == 0; }
};

// 鳴き声の種類。気分の優先順位は FaceComposer と揃えてある。
// 顔と声が別のことを言っていると、何を感じているのか読めなくなるため。
enum class VoiceMood {
  Neutral,
  Happy,
  Dizzy,
  Angry,
  Sleepy,
};

class VoiceComposer {
 public:
  // 鳴き声が続けて出すぎないための最短間隔 [ms]
  static constexpr uint32_t kMinIntervalMs = 600;

  // 眠っているときの寝息の間隔 [ms]
  static constexpr uint32_t kBreathIntervalMs = 7000;

  // 気分が変わった節目と、つつき・持ち上げのときだけ鳴く。
  // 鳴かないときは count == 0 の VoiceCue を返す。
  VoiceCue update(MotionEvent event, const MoodState &mood, uint32_t tMs);

  VoiceMood mood() const { return mood_; }

 private:
  VoiceMood classify(const MoodState &mood) const;

  VoiceMood mood_ = VoiceMood::Neutral;
  bool started_ = false;

  uint32_t lastCueMs_ = 0;
  uint32_t lastBreathMs_ = 0;
};

}  // namespace pet
