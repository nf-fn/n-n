// 鳴き声のテスト
//
// この顔は言葉を話さない。ピッチと長さだけで感情を表すので、
// 「上がる/下がる」「高い/低い」といった性質をテストで固定する。
//
// 鳴りすぎないことも同じくらい重要。撫でているあいだ鳴り続けたら耳障りになる。

#include <unity.h>

#include "Types.h"
#include "Voice.h"

using pet::MoodState;
using pet::MotionEvent;
using pet::VoiceComposer;
using pet::VoiceCue;
using pet::VoiceMood;

namespace {

// 起動直後の「黙って始める」ぶんを消費する
void prime(VoiceComposer &voice, uint32_t tMs = 0) {
  voice.update(MotionEvent::None, MoodState{}, tMs);
}

MoodState happy() {
  MoodState m;
  m.valence = 0.9f;
  return m;
}

MoodState angry() {
  MoodState m;
  m.anger = 0.9f;
  return m;
}

MoodState dizzy() {
  MoodState m;
  m.dizziness = 0.9f;
  return m;
}

MoodState sleepy() {
  MoodState m;
  m.sleepiness = 0.9f;
  return m;
}

int lowestFreq(const VoiceCue &cue) {
  int lo = 100000;
  for (int i = 0; i < cue.count; ++i) {
    if (cue.notes[i].freqHz > 0 && cue.notes[i].freqHz < lo) {
      lo = cue.notes[i].freqHz;
    }
  }
  return lo;
}

int highestFreq(const VoiceCue &cue) {
  int hi = 0;
  for (int i = 0; i < cue.count; ++i) {
    if (cue.notes[i].freqHz > hi) {
      hi = cue.notes[i].freqHz;
    }
  }
  return hi;
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- 鳴くこと ---

void test_tap_makes_a_sound() {
  VoiceComposer voice;
  prime(voice);

  const VoiceCue cue = voice.update(MotionEvent::Tap, MoodState{}, 1000);

  TEST_ASSERT_TRUE_MESSAGE(!cue.empty(), "つついても鳴かない");
}

void test_lift_rises_in_pitch() {
  VoiceComposer voice;
  prime(voice);

  const VoiceCue cue = voice.update(MotionEvent::Lift, MoodState{}, 1000);

  TEST_ASSERT_TRUE(cue.count >= 2);
  // 持ち上げられて「ん？」と上を向く。最後が一番高い。
  TEST_ASSERT_TRUE_MESSAGE(
      cue.notes[cue.count - 1].freqHz > cue.notes[0].freqHz,
      "持ち上げの声が上がっていない");
}

void test_becoming_happy_makes_a_rising_sound() {
  VoiceComposer voice;
  prime(voice);

  const VoiceCue cue = voice.update(MotionEvent::None, happy(), 2000);

  TEST_ASSERT_TRUE(!cue.empty());
  TEST_ASSERT_TRUE_MESSAGE(
      cue.notes[cue.count - 1].freqHz > cue.notes[0].freqHz,
      "喜びの声が上がっていない");
}

void test_anger_is_lower_than_happiness() {
  VoiceComposer a;
  prime(a);
  const VoiceCue angryCue = a.update(MotionEvent::None, angry(), 2000);

  VoiceComposer h;
  prime(h);
  const VoiceCue happyCue = h.update(MotionEvent::None, happy(), 2000);

  TEST_ASSERT_TRUE(!angryCue.empty());
  TEST_ASSERT_TRUE(!happyCue.empty());
  TEST_ASSERT_TRUE_MESSAGE(highestFreq(angryCue) < lowestFreq(happyCue),
                           "怒りの声が喜びより高い");
}

void test_dizziness_wobbles_up_and_down() {
  VoiceComposer voice;
  prime(voice);

  const VoiceCue cue = voice.update(MotionEvent::None, dizzy(), 2000);

  TEST_ASSERT_TRUE(cue.count >= 4);

  // 上がって下がってを繰り返すこと。単調だと揺れて聞こえない。
  bool sawUp = false;
  bool sawDown = false;
  for (int i = 1; i < cue.count; ++i) {
    if (cue.notes[i].freqHz > cue.notes[i - 1].freqHz) sawUp = true;
    if (cue.notes[i].freqHz < cue.notes[i - 1].freqHz) sawDown = true;
  }
  TEST_ASSERT_TRUE_MESSAGE(sawUp && sawDown, "めまいの声が揺れていない");
}

void test_sleepiness_falls_in_pitch() {
  VoiceComposer voice;
  prime(voice);

  const VoiceCue cue = voice.update(MotionEvent::None, sleepy(), 2000);

  TEST_ASSERT_TRUE(cue.count >= 2);
  TEST_ASSERT_TRUE_MESSAGE(
      cue.notes[cue.count - 1].freqHz < cue.notes[0].freqHz,
      "眠気の声が下がっていない");
}

// --- 鳴きすぎないこと ---

void test_repeats_at_intervals_while_the_mood_continues() {
  VoiceComposer voice;
  prime(voice);

  // 機嫌が良くなった瞬間に 1 回鳴く
  TEST_ASSERT_TRUE(!voice.update(MotionEvent::None, happy(), 2000).empty());

  // そのあとも間を置いて鳴く。鳴りっぱなしでも無言でもない。
  int extra = 0;
  for (uint32_t t = 2100; t < 32000; t += 100) {
    if (!voice.update(MotionEvent::None, happy(), t).empty()) {
      ++extra;
    }
  }

  // 30 秒を 4.5 秒間隔なら 6 回前後
  TEST_ASSERT_TRUE_MESSAGE(extra >= 4, "撫でられ続けても黙ったまま");
  TEST_ASSERT_TRUE_MESSAGE(extra <= 9, "鳴きすぎている");
}

// 平静なときは黙っている。何もされていないのに鳴くとうるさい。
void test_stays_silent_when_neutral() {
  VoiceComposer voice;
  prime(voice);

  int count = 0;
  for (uint32_t t = 100; t < 60000; t += 100) {
    if (!voice.update(MotionEvent::None, MoodState{}, t).empty()) {
      ++count;
    }
  }
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, count, "平静なのに鳴いている");
}

void test_respects_a_minimum_interval() {
  VoiceComposer voice;
  prime(voice);

  TEST_ASSERT_TRUE(!voice.update(MotionEvent::Tap, MoodState{}, 1000).empty());

  // 最短間隔の内側では鳴かない
  TEST_ASSERT_TRUE_MESSAGE(
      voice.update(MotionEvent::Tap, MoodState{}, 1100).empty(),
      "連打で鳴きっぱなしになる");

  // 間隔が空けば鳴く
  TEST_ASSERT_TRUE(
      !voice.update(MotionEvent::Tap, MoodState{},
                    1000 + VoiceComposer::kMinIntervalMs + 10).empty());
}

void test_returning_to_neutral_is_silent() {
  VoiceComposer voice;
  prime(voice);
  voice.update(MotionEvent::None, happy(), 2000);

  // 機嫌が戻るときは黙る。何かあるたびに鳴くとうるさい。
  const VoiceCue cue = voice.update(MotionEvent::None, MoodState{}, 5000);

  TEST_ASSERT_TRUE_MESSAGE(cue.empty(), "我に返るときに鳴いている");
}

void test_sleeping_breathes_occasionally() {
  VoiceComposer voice;
  prime(voice);
  voice.update(MotionEvent::None, sleepy(), 2000);  // 眠りに入る声

  int breaths = 0;
  for (uint32_t t = 2100; t < 62000; t += 100) {
    if (!voice.update(MotionEvent::None, sleepy(), t).empty()) {
      ++breaths;
    }
  }

  // 1 分のあいだに数回。無音でも多すぎでもない。
  TEST_ASSERT_TRUE_MESSAGE(breaths >= 4, "寝息が出ていない");
  TEST_ASSERT_TRUE_MESSAGE(breaths <= 12, "寝息が多すぎる");
}

void test_every_note_has_a_duration() {
  VoiceComposer voice;
  prime(voice);

  const MoodState moods[] = {happy(), angry(), dizzy(), sleepy()};
  uint32_t t = 2000;
  for (const MoodState &m : moods) {
    VoiceComposer v;
    prime(v);
    const VoiceCue cue = v.update(MotionEvent::None, m, t);
    for (int i = 0; i < cue.count; ++i) {
      TEST_ASSERT_TRUE_MESSAGE(cue.notes[i].durMs > 0,
                               "長さ 0 の音符が混ざっている");
    }
    t += 1000;
  }
}

// 感情の強さが音の大きさに出ること。
// 怒り 0.5 と 1.0 が同じ音で鳴ると、どれだけ怒っているのか伝わらない。
void test_intensity_follows_the_mood() {
  MoodState mild;
  mild.anger = 0.5f;
  MoodState furious;
  furious.anger = 1.0f;

  VoiceComposer a;
  prime(a);
  const VoiceCue mildCue = a.update(MotionEvent::None, mild, 2000);

  VoiceComposer b;
  prime(b);
  const VoiceCue furiousCue = b.update(MotionEvent::None, furious, 2000);

  TEST_ASSERT_TRUE(!mildCue.empty() && !furiousCue.empty());
  TEST_ASSERT_TRUE_MESSAGE(furiousCue.intensity > mildCue.intensity,
                           "怒りの強さが声に出ていない");
}

// 弱い感情でも消え入らないこと。聞こえないなら鳴かないのと同じ。
void test_weak_moods_are_still_audible() {
  MoodState barely;
  barely.anger = 0.46f;  // 閾値をぎりぎり超えた程度

  VoiceComposer voice;
  prime(voice);
  const VoiceCue cue = voice.update(MotionEvent::None, barely, 2000);

  TEST_ASSERT_TRUE(!cue.empty());
  TEST_ASSERT_TRUE_MESSAGE(cue.intensity > 0.4f, "弱い感情の声が小さすぎる");
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_tap_makes_a_sound);
  RUN_TEST(test_lift_rises_in_pitch);
  RUN_TEST(test_becoming_happy_makes_a_rising_sound);
  RUN_TEST(test_anger_is_lower_than_happiness);
  RUN_TEST(test_dizziness_wobbles_up_and_down);
  RUN_TEST(test_sleepiness_falls_in_pitch);
  RUN_TEST(test_repeats_at_intervals_while_the_mood_continues);
  RUN_TEST(test_stays_silent_when_neutral);
  RUN_TEST(test_respects_a_minimum_interval);
  RUN_TEST(test_returning_to_neutral_is_silent);
  RUN_TEST(test_sleeping_breathes_occasionally);
  RUN_TEST(test_every_note_has_a_duration);
  RUN_TEST(test_intensity_follows_the_mood);
  RUN_TEST(test_weak_moods_are_still_audible);
  return UNITY_END();
}
