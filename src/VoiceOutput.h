// [I/O] VoiceCue を Atomic Echo Base のスピーカーで鳴らす層。
//
// tone() の矩形波ではブザーに聞こえるため、VoiceSynth で波形を組み立てて
// PCM として流す。鳴き声 1 回ぶんをまとめて描くので、再生中に loop を
// 止める必要がない。
#pragma once

#include <M5Unified.h>

#include "Voice.h"
#include "VoiceSynth.h"

namespace pet {

class VoiceOutput {
 public:
  bool begin(uint8_t volume);

  // 鳴らし始める。再生中なら今の鳴き声を止めて差し替える。
  // 新しい反応のほうが古い反応より優先されるべきなので。
  void play(const VoiceCue &cue);

  bool isSpeaking() const;

 private:
  VoiceSynth synth_;
  int16_t buffer_[VoiceSynth::kMaxSamples];
  bool ready_ = false;
};

}  // namespace pet
