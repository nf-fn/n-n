// 気分の遷移テスト (フェーズ 3)
//
// 触られ方の列と経過時間から MoodState を作る層。表情の説得力は
// ここの減衰カーブで決まるので、性質をテストで固定しておく。

#include <unity.h>

#include "Mood.h"
#include "Types.h"

using pet::Activity;
using pet::Mood;
using pet::MoodState;
using pet::MotionEvent;
using pet::Posture;

namespace {

constexpr float kDt = 0.01f;  // 100Hz

// 画面を上に向けて机に置いた姿勢。既定の状態。
Posture faceUp() {
  Posture p;
  p.upZ = 1.0f;
  p.faceUp = true;
  return p;
}

// 画面を下に伏せた姿勢。
Posture faceDown() {
  Posture p;
  p.upZ = -1.0f;
  p.faceUp = false;
  p.faceDown = true;
  return p;
}

// 何もせずに時間を進める
void idle(Mood &mood, float seconds, const Posture &posture = faceUp()) {
  const int steps = static_cast<int>(seconds / kDt);
  for (int i = 0; i < steps; ++i) {
    mood.update(MotionEvent::None, Activity::Quiet, posture, kDt);
  }
}

// ある状態を続ける
void hold(Mood &mood, Activity activity, float seconds) {
  const int steps = static_cast<int>(seconds / kDt);
  for (int i = 0; i < steps; ++i) {
    mood.update(MotionEvent::None, activity, faceUp(), kDt);
  }
}

bool inRange(float v, float lo, float hi) { return v >= lo && v <= hi; }

}  // namespace

void setUp() {}
void tearDown() {}

// --- つつき ---

void test_tap_spikes_arousal() {
  Mood mood;
  const float before = mood.state().arousal;

  mood.update(MotionEvent::Tap, Activity::Quiet, faceUp(), kDt);

  TEST_ASSERT_TRUE_MESSAGE(mood.state().arousal > before + 0.2f,
                           "つついても覚醒度が跳ねていない");
}

void test_arousal_returns_after_tap() {
  Mood mood;
  const float baseline = mood.state().arousal;

  mood.update(MotionEvent::Tap, Activity::Quiet, faceUp(), kDt);
  const float peak = mood.state().arousal;

  idle(mood, 15.0f);

  TEST_ASSERT_TRUE(mood.state().arousal < peak);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, baseline, mood.state().arousal);
}

// --- 持ち上げ ---

void test_lift_wakes_up() {
  Mood mood;
  idle(mood, 120.0f);  // うとうとさせる
  TEST_ASSERT_TRUE_MESSAGE(mood.state().sleepiness > 0.5f,
                           "放置しても眠くなっていない");

  mood.update(MotionEvent::Lift, Activity::Quiet, faceUp(), kDt);

  TEST_ASSERT_TRUE_MESSAGE(mood.state().sleepiness < 0.5f,
                           "持ち上げても目が覚めていない");
}

// --- 振り ---

void test_shaking_accumulates_dizziness() {
  Mood mood;
  hold(mood, Activity::Shake, 3.0f);

  TEST_ASSERT_TRUE_MESSAGE(mood.state().dizziness > 0.5f,
                           "振ってもめまいが溜まらない");
}

void test_dizziness_fades_after_shaking_stops() {
  Mood mood;
  hold(mood, Activity::Shake, 3.0f);
  const float peak = mood.state().dizziness;

  idle(mood, 20.0f);

  TEST_ASSERT_TRUE(mood.state().dizziness < peak * 0.3f);
}

void test_prolonged_shaking_makes_it_angry() {
  Mood mood;
  hold(mood, Activity::Shake, 4.0f);

  TEST_ASSERT_TRUE_MESSAGE(mood.state().anger > 0.5f,
                           "振り続けても怒らない");
}

void test_brief_shaking_does_not_anger() {
  Mood mood;
  hold(mood, Activity::Shake, 1.0f);

  TEST_ASSERT_TRUE_MESSAGE(mood.state().anger < 0.3f,
                           "少し振っただけで怒っている");
}

// --- 撫で ---

void test_stroking_raises_valence() {
  Mood mood;
  hold(mood, Activity::Stroke, 3.0f);

  TEST_ASSERT_TRUE_MESSAGE(mood.state().valence > 0.4f,
                           "撫でても機嫌が良くならない");
}

void test_stroking_calms_anger() {
  Mood mood;
  hold(mood, Activity::Shake, 8.0f);
  const float angry = mood.state().anger;

  hold(mood, Activity::Stroke, 6.0f);

  TEST_ASSERT_TRUE_MESSAGE(mood.state().anger < angry * 0.5f,
                           "撫でても怒りが収まらない");
}

// --- 放置 ---

void test_idle_makes_it_sleepy() {
  Mood mood;
  idle(mood, 60.0f);

  TEST_ASSERT_TRUE(mood.state().sleepiness > 0.5f);
}

void test_stroking_keeps_it_awake() {
  Mood mood;
  hold(mood, Activity::Stroke, 60.0f);

  TEST_ASSERT_TRUE_MESSAGE(mood.state().sleepiness < 0.3f,
                           "撫でているのに眠くなっている");
}

// --- 暴走しないこと ---
//
// 各値は連続量なので、積み上げ続けると発散する余地がある。
// 長時間どの入力を与えても範囲に収まることを固定しておく。

void test_values_stay_in_range_under_sustained_shaking() {
  Mood mood;
  hold(mood, Activity::Shake, 600.0f);

  const MoodState &s = mood.state();
  TEST_ASSERT_TRUE(inRange(s.arousal, 0.0f, 1.0f));
  TEST_ASSERT_TRUE(inRange(s.valence, -1.0f, 1.0f));
  TEST_ASSERT_TRUE(inRange(s.dizziness, 0.0f, 1.0f));
  TEST_ASSERT_TRUE(inRange(s.anger, 0.0f, 1.0f));
  TEST_ASSERT_TRUE(inRange(s.sleepiness, 0.0f, 1.0f));
}

void test_values_stay_in_range_under_sustained_stroking() {
  Mood mood;
  hold(mood, Activity::Stroke, 600.0f);

  const MoodState &s = mood.state();
  TEST_ASSERT_TRUE(inRange(s.arousal, 0.0f, 1.0f));
  TEST_ASSERT_TRUE(inRange(s.valence, -1.0f, 1.0f));
  TEST_ASSERT_TRUE(inRange(s.dizziness, 0.0f, 1.0f));
  TEST_ASSERT_TRUE(inRange(s.anger, 0.0f, 1.0f));
  TEST_ASSERT_TRUE(inRange(s.sleepiness, 0.0f, 1.0f));
}

void test_everything_settles_when_left_alone() {
  Mood mood;
  hold(mood, Activity::Shake, 10.0f);
  idle(mood, 600.0f);

  const MoodState &s = mood.state();
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, s.dizziness);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, s.anger);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, s.valence);
}

// --- 伏せて寝かしつける ---
//
// 自然な眠気 (40 秒) とは別枠の、意図的なジェスチャーとして成立させる。

void test_placing_it_face_down_puts_it_to_sleep_quickly() {
  Mood mood;
  idle(mood, 8.0f, faceDown());

  TEST_ASSERT_TRUE_MESSAGE(mood.state().sleepiness > 0.6f,
                           "伏せても眠らない");
}

// 伏せていなければ、同じ時間では眠らない。差が付いていなければ意味がない。
void test_face_up_does_not_sleep_that_fast() {
  Mood mood;
  idle(mood, 8.0f, faceUp());

  TEST_ASSERT_TRUE_MESSAGE(mood.state().sleepiness < 0.3f,
                           "伏せていないのに早く眠っている");
}

// 起こせば覚める。伏せる/起こすが対になっていること。
void test_lifting_it_wakes_it_from_face_down() {
  Mood mood;
  idle(mood, 8.0f, faceDown());
  TEST_ASSERT_TRUE(mood.state().sleepiness > 0.6f);

  mood.update(MotionEvent::Lift, Activity::Quiet, faceUp(), kDt);

  TEST_ASSERT_TRUE_MESSAGE(mood.state().sleepiness < 0.5f,
                           "起こしても覚めない");
}

// 伏せた状態で振られても、寝かしつけにはならない。
// 触られている最中は Quiet ではないので、そもそも対象外であること。
void test_face_down_while_shaken_does_not_sleep() {
  Mood mood;
  const int steps = static_cast<int>(8.0f / kDt);
  for (int i = 0; i < steps; ++i) {
    mood.update(MotionEvent::None, Activity::Shake, faceDown(), kDt);
  }

  TEST_ASSERT_TRUE_MESSAGE(mood.state().sleepiness < 0.3f,
                           "振られているのに眠っている");
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_tap_spikes_arousal);
  RUN_TEST(test_arousal_returns_after_tap);
  RUN_TEST(test_lift_wakes_up);
  RUN_TEST(test_shaking_accumulates_dizziness);
  RUN_TEST(test_dizziness_fades_after_shaking_stops);
  RUN_TEST(test_prolonged_shaking_makes_it_angry);
  RUN_TEST(test_brief_shaking_does_not_anger);
  RUN_TEST(test_stroking_raises_valence);
  RUN_TEST(test_stroking_calms_anger);
  RUN_TEST(test_idle_makes_it_sleepy);
  RUN_TEST(test_stroking_keeps_it_awake);
  RUN_TEST(test_values_stay_in_range_under_sustained_shaking);
  RUN_TEST(test_values_stay_in_range_under_sustained_stroking);
  RUN_TEST(test_everything_settles_when_left_alone);
  RUN_TEST(test_placing_it_face_down_puts_it_to_sleep_quickly);
  RUN_TEST(test_face_up_does_not_sleep_that_fast);
  RUN_TEST(test_lifting_it_wakes_it_from_face_down);
  RUN_TEST(test_face_down_while_shaken_does_not_sleep);
  return UNITY_END();
}
