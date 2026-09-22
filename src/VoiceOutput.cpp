#include "VoiceOutput.h"

namespace pet {

bool VoiceOutput::begin(uint8_t volume) {
  if (!M5.Speaker.isEnabled()) {
    return false;
  }
  M5.Speaker.setVolume(volume);
  ready_ = true;
  return true;
}

void VoiceOutput::play(const VoiceCue &cue) {
  if (!ready_ || cue.empty()) {
    return;
  }

  const size_t samples =
      synth_.render(cue, buffer_, VoiceSynth::kMaxSamples);
  if (samples == 0) {
    return;
  }

  // stop_current_sound = true。鳴いている途中で新しい反応が来たら、
  // 古いほうを捨てて今の反応を出す。
  M5.Speaker.playRaw(buffer_, samples, VoiceSynth::kSampleRate,
                     /*stereo=*/false, /*repeat=*/1, /*channel=*/-1,
                     /*stop_current_sound=*/true);
}

bool VoiceOutput::isSpeaking() const {
  return ready_ && M5.Speaker.isPlaying();
}

}  // namespace pet
