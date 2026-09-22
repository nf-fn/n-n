#include "VoiceOutput.h"

#include <esp_heap_caps.h>

namespace pet {
namespace {

constexpr size_t kBufferBytes = VoiceSynth::kMaxSamples * sizeof(int16_t);

}  // namespace

VoiceOutput::~VoiceOutput() {
  if (buffer_ != nullptr) {
    heap_caps_free(buffer_);
  }
}

bool VoiceOutput::begin(uint8_t volume) {
  if (!M5.Speaker.isEnabled()) {
    return false;
  }

  // 52KB ある。内蔵 RAM は他に使い道があるので PSRAM から取る。
  buffer_ = static_cast<int16_t *>(
      heap_caps_malloc(kBufferBytes, MALLOC_CAP_SPIRAM));
  if (buffer_ == nullptr) {
    // PSRAM が無い個体や確保に失敗した場合は内蔵 RAM に落とす。
    // 鳴らないより内蔵 RAM を使うほうがまし。
    buffer_ = static_cast<int16_t *>(heap_caps_malloc(kBufferBytes,
                                                      MALLOC_CAP_8BIT));
  }
  if (buffer_ == nullptr) {
    return false;
  }

  M5.Speaker.setVolume(volume);
  ready_ = true;
  return true;
}

void VoiceOutput::play(const VoiceCue &cue) {
  if (!ready_ || buffer_ == nullptr || cue.empty()) {
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
