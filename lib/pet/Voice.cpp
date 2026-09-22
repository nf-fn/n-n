#include "Voice.h"

namespace pet {
namespace {

// 気分を声に落とす閾値。FaceComposer と同じ値にして、顔と声を一致させる。
constexpr float kDizzyThreshold = 0.45f;
constexpr float kAngerThreshold = 0.45f;
constexpr float kSleepyThreshold = 0.60f;
constexpr float kHappyThreshold = 0.45f;

VoiceCue make(const VoiceNote *notes, int count) {
  VoiceCue cue;
  cue.count = count < kMaxVoiceNotes ? count : kMaxVoiceNotes;
  for (int i = 0; i < cue.count; ++i) {
    cue.notes[i] = notes[i];
  }
  return cue;
}

// つつかれた驚き。短く高く跳ねる。
constexpr VoiceNote kTap[] = {{1400, 55}, {1900, 45}};

// 持ち上げられて「ん？」と上を向く。上がって終わる。
constexpr VoiceNote kLift[] = {{880, 70}, {1180, 70}, {1560, 110}};

// 機嫌が良い。柔らかく上がる。
constexpr VoiceNote kHappy[] = {{1050, 85}, {1320, 85}, {1570, 140}};

// めまい。音程が上下に揺れる。
constexpr VoiceNote kDizzy[] = {{900, 65}, {700, 65}, {900, 65}, {700, 65},
                                {880, 100}};

// 怒り。低く短く詰まる。
constexpr VoiceNote kAngry[] = {{220, 150}, {175, 210}};

// 眠くなってきた。ゆっくり下がる。
constexpr VoiceNote kSleepy[] = {{700, 180}, {560, 240}};

// 寝息。吸って吐く。
constexpr VoiceNote kBreath[] = {{420, 260}, {0, 110}, {350, 200}};

template <int N>
VoiceCue cueOf(const VoiceNote (&notes)[N]) {
  return make(notes, N);
}

}  // namespace

VoiceMood VoiceComposer::classify(const MoodState &mood) const {
  if (mood.dizziness >= kDizzyThreshold) {
    return VoiceMood::Dizzy;
  }
  if (mood.anger >= kAngerThreshold) {
    return VoiceMood::Angry;
  }
  if (mood.sleepiness >= kSleepyThreshold) {
    return VoiceMood::Sleepy;
  }
  if (mood.valence >= kHappyThreshold) {
    return VoiceMood::Happy;
  }
  return VoiceMood::Neutral;
}

VoiceCue VoiceComposer::update(MotionEvent event, const MoodState &mood,
                               uint32_t tMs) {
  const VoiceMood now = classify(mood);
  const VoiceMood was = mood_;
  mood_ = now;

  // 起動直後は「変わった」と見なさない。黙って始める。
  if (!started_) {
    started_ = true;
    lastCueMs_ = tMs;
    lastBreathMs_ = tMs;
    return VoiceCue{};
  }

  const bool tooSoon = (tMs - lastCueMs_) < kMinIntervalMs;

  // つつきと持ち上げは即座に返す。反応の速さが命なので、
  // 気分の変化より優先する。
  if (event == MotionEvent::Tap && !tooSoon) {
    lastCueMs_ = tMs;
    return cueOf(kTap);
  }
  if (event == MotionEvent::Lift && !tooSoon) {
    lastCueMs_ = tMs;
    return cueOf(kLift);
  }

  if (tooSoon) {
    return VoiceCue{};
  }

  // 気分が変わった節目だけ鳴く。同じ気分が続くあいだは黙っている。
  // 撫でているあいだ鳴り続けると耳障りになるため。
  if (now != was) {
    lastCueMs_ = tMs;
    lastBreathMs_ = tMs;
    switch (now) {
      case VoiceMood::Happy: return cueOf(kHappy);
      case VoiceMood::Dizzy: return cueOf(kDizzy);
      case VoiceMood::Angry: return cueOf(kAngry);
      case VoiceMood::Sleepy: return cueOf(kSleepy);
      case VoiceMood::Neutral: break;  // 我に返るときは黙る
    }
    return VoiceCue{};
  }

  // 眠っているあいだだけ、ときどき寝息を立てる。
  if (now == VoiceMood::Sleepy &&
      (tMs - lastBreathMs_) >= kBreathIntervalMs) {
    lastCueMs_ = tMs;
    lastBreathMs_ = tMs;
    return cueOf(kBreath);
  }

  return VoiceCue{};
}

}  // namespace pet
