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
  cue_ = cue;
  index_ = 0;
  nextNoteMs_ = 0;  // 次の update で即座に 1 音目を出す
}

void VoiceOutput::update(uint32_t nowMs) {
  if (!ready_ || index_ >= cue_.count) {
    return;
  }
  if (nextNoteMs_ != 0 && nowMs < nextNoteMs_) {
    return;
  }

  const VoiceNote &note = cue_.notes[index_];
  if (note.freqHz > 0) {
    M5.Speaker.tone(note.freqHz, note.durMs);
  }
  // 周波数 0 は無音。鳴らさずに長さぶん待つ。

  nextNoteMs_ = nowMs + note.durMs;
  ++index_;
}

}  // namespace pet
