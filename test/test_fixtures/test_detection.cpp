// 実機で録ったモーションに対する検出テスト (フェーズ 2)
//
// 各動作について「正例で検出できる」と「他の動作で誤検出しない」の両方を
// 確かめる。後者があるから、閾値をいじっても過去の挙動が壊れない。
//
// 一番重要なのは walk を撫でと誤判定しないこと。線形加速度が重なるため、
// 大きさだけでは分けられない (撫で 0.069-0.089 / 歩行 0.091-0.225)。
// 角速度で分離する (撫で 9-15 deg/s / 歩行 28-55 deg/s)。

#include <unity.h>

#include <cstdio>

#include <string>
#include <vector>

#include "MotionAnalyzer.h"
#include "Types.h"
#include "fixture_loader.h"

using pet::Activity;
using pet::ImuSample;
using pet::MotionAnalyzer;
using pet::MotionEvent;
using pet_test::loadFixture;

namespace {

struct Summary {
  int samples = 0;
  int taps = 0;
  int lifts = 0;
  int quietFrames = 0;
  int strokeFrames = 0;
  int heldFrames = 0;
  int shakeFrames = 0;

  bool sawStroke() const { return strokeFrames > 0; }
  bool sawShake() const { return shakeFrames > 0; }
};

Summary run(const std::string &name) {
  const std::vector<ImuSample> samples = loadFixture(name);
  TEST_ASSERT_TRUE_MESSAGE(!samples.empty(),
                           ("フィクスチャが空: " + name).c_str());

  MotionAnalyzer analyzer;
  Summary s;
  s.samples = static_cast<int>(samples.size());

  int restFrames = 0;
  int upwardPeaks = 0;
  float maxUpward = 0.0f;
  bool wasAboveUpward = false;

  for (const ImuSample &sample : samples) {
    analyzer.update(sample);

    if (analyzer.linearAccel() < 0.02f) {
      ++restFrames;
    }
    if (analyzer.upwardAccel() > maxUpward) {
      maxUpward = analyzer.upwardAccel();
    }
    const bool above = analyzer.upwardAccel() > 0.35f;
    if (above && !wasAboveUpward) {
      ++upwardPeaks;
    }
    wasAboveUpward = above;

    switch (analyzer.event()) {
      case MotionEvent::Tap: ++s.taps; break;
      case MotionEvent::Lift: ++s.lifts; break;
      case MotionEvent::None: break;
    }

    switch (analyzer.activity()) {
      case Activity::Quiet: ++s.quietFrames; break;
      case Activity::Stroke: ++s.strokeFrames; break;
      case Activity::Held: ++s.heldFrames; break;
      case Activity::Shake: ++s.shakeFrames; break;
    }
  }

  // 閾値を調整するときに効く内訳。テストが落ちたときに読む。
  std::printf(
      "  [%-7s] n=%4d tap=%2d lift=%2d | quiet=%4d stroke=%4d held=%4d "
      "shake=%4d | rest=%4d up>.35=%2d upmax=%.2f\n",
      name.c_str(), s.samples, s.taps, s.lifts, s.quietFrames, s.strokeFrames,
      s.heldFrames, s.shakeFrames, restFrames, upwardPeaks, maxUpward);

  return s;
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- idle: 何も起きてはいけない ---

void test_idle_detects_nothing() {
  const Summary s = run("idle");

  TEST_ASSERT_EQUAL_INT(0, s.taps);
  TEST_ASSERT_EQUAL_INT(0, s.lifts);
  TEST_ASSERT_FALSE(s.sawStroke());
  TEST_ASSERT_FALSE(s.sawShake());
  // ほぼ全フレームが静止と判定されること
  TEST_ASSERT_TRUE(s.quietFrames > s.samples * 9 / 10);
}

// --- tap: つついた回数が取れ、撫でや振りとは判定されない ---

void test_tap_is_detected() {
  const Summary s = run("tap");
  TEST_ASSERT_TRUE_MESSAGE(s.taps >= 3, "つつきが 3 回以上取れていない");
}

void test_tap_is_not_stroke_or_shake() {
  const Summary s = run("tap");
  TEST_ASSERT_FALSE(s.sawStroke());
  TEST_ASSERT_FALSE(s.sawShake());
}

// --- stroke: 撫でとして検出され、つつきは出ない ---

void test_stroke_is_detected() {
  const Summary s = run("stroke");
  // 録音の大半が撫でている時間なので、半分以上は撫でと判定されてほしい
  TEST_ASSERT_TRUE_MESSAGE(s.strokeFrames > s.samples / 2,
                           "撫でと判定された時間が短すぎる");
}

void test_stroke_produces_no_taps() {
  const Summary s = run("stroke");
  TEST_ASSERT_EQUAL_INT(0, s.taps);
}

void test_stroke_is_not_shake() {
  const Summary s = run("stroke");
  TEST_ASSERT_FALSE(s.sawShake());
}

// --- shake: 振りとして検出され、撫でとは判定されない ---

void test_shake_is_detected() {
  const Summary s = run("shake");
  TEST_ASSERT_TRUE_MESSAGE(s.shakeFrames > s.samples / 2,
                           "振りと判定された時間が短すぎる");
}

void test_shake_is_not_stroke() {
  const Summary s = run("shake");
  TEST_ASSERT_FALSE(s.sawStroke());
}

// --- lift: 持ち上げが取れる ---

void test_lift_is_detected() {
  const Summary s = run("lift");
  TEST_ASSERT_TRUE_MESSAGE(s.lifts >= 2, "持ち上げが 2 回以上取れていない");
}

// 机に置いたまま撫でても持ち上げにはならない。
// ゆっくりした持ち上げを状態遷移で拾うようにしたため、撫でへの遷移を
// 巻き込まないことを固定しておく。
void test_stroke_produces_no_lifts() {
  const Summary s = run("stroke");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, s.lifts, "撫でを持ち上げと誤検出している");
}

// つついても持ち上げにはならない。
void test_tap_produces_no_lifts() {
  const Summary s = run("tap");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, s.lifts, "つつきを持ち上げと誤検出している");
}

// 置いたまま放置している間は何も起きない。
void test_idle_produces_no_lifts() {
  const Summary s = run("idle");
  TEST_ASSERT_EQUAL_INT(0, s.lifts);
}

// --- walk: これが最重要の負例 ---
//
// 持ち歩くおもちゃなので、歩いただけで撫でられたと判定されたら破綻する。

void test_walk_is_never_stroke() {
  const Summary s = run("walk");
  TEST_ASSERT_FALSE_MESSAGE(s.sawStroke(),
                            "歩行を撫でと誤判定している");
}

void test_walk_is_never_shake() {
  const Summary s = run("walk");
  TEST_ASSERT_FALSE_MESSAGE(s.sawShake(), "歩行を振りと誤判定している");
}

void test_walk_produces_no_taps() {
  const Summary s = run("walk");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, s.taps, "歩行でつつきを誤検出している");
}

void test_walk_is_classified_as_held() {
  const Summary s = run("walk");
  TEST_ASSERT_TRUE_MESSAGE(s.heldFrames > s.samples / 2,
                           "手に持たれていると判定されていない");
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_detects_nothing);
  RUN_TEST(test_tap_is_detected);
  RUN_TEST(test_tap_is_not_stroke_or_shake);
  RUN_TEST(test_stroke_is_detected);
  RUN_TEST(test_stroke_produces_no_taps);
  RUN_TEST(test_stroke_is_not_shake);
  RUN_TEST(test_shake_is_detected);
  RUN_TEST(test_shake_is_not_stroke);
  RUN_TEST(test_lift_is_detected);
  RUN_TEST(test_stroke_produces_no_lifts);
  RUN_TEST(test_tap_produces_no_lifts);
  RUN_TEST(test_idle_produces_no_lifts);
  RUN_TEST(test_walk_is_never_stroke);
  RUN_TEST(test_walk_is_never_shake);
  RUN_TEST(test_walk_produces_no_taps);
  RUN_TEST(test_walk_is_classified_as_held);
  return UNITY_END();
}
