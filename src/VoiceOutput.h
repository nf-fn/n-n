// [I/O] VoiceCue を Atomic Echo Base のスピーカーで鳴らす層。
//
// 音符を 1 つずつ順に鳴らす。tone() は鳴り終わるのを待たないので、
// 前の音符の長さが過ぎたら次を出す、という形で進める。
// loop を止めないことが条件なので、待ちは入れない。
#pragma once

#include <M5Unified.h>

#include "Voice.h"

namespace pet {

class VoiceOutput {
 public:
  bool begin(uint8_t volume);

  // 鳴らし始める。再生中に来た場合は今の鳴き声を捨てて差し替える。
  // 新しい反応のほうが古い反応より優先されるべきなので。
  void play(const VoiceCue &cue);

  // 毎ループ呼ぶ。次の音符へ進める。
  void update(uint32_t nowMs);

  bool isSpeaking() const { return index_ < cue_.count; }

 private:
  VoiceCue cue_;
  int index_ = 0;
  uint32_t nextNoteMs_ = 0;
  bool ready_ = false;
};

}  // namespace pet
