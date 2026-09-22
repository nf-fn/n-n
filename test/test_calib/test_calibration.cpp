// キャリブレーションのテスト
//
// 一番怖いのは「測り直したら検出が壊れる」こと。実機で録った動作から
// 閾値を導出し、その閾値で元の動作がまだ検出でき、かつ最重要の負例
// (手に持って動かしている walk) を撫でと誤判定しないことを確かめる。

#include <unity.h>

#include <string>
#include <vector>

#include "Calibration.h"
#include "MotionAnalyzer.h"
#include "Types.h"
#include "fixture_loader.h"

using pet::Activity;
using pet::CalibrationSet;
using pet::Calibrator;
using pet::CalibTarget;
using pet::ImuSample;
using pet::MotionAnalyzer;
using pet::MotionEvent;
using pet::Thresholds;
using pet_test::loadFixture;

namespace {

// フィクスチャを測定に流し、導出結果を得る
Calibrator::Result calibrateFrom(CalibTarget target, const std::string &name) {
  const std::vector<ImuSample> samples = loadFixture(name);
  TEST_ASSERT_TRUE_MESSAGE(!samples.empty(),
                           ("フィクスチャが空: " + name).c_str());

  Calibrator c;
  c.begin(target);
  for (const ImuSample &s : samples) {
    c.feed(s);
  }
  return c.finish();
}

struct Counts {
  int taps = 0;
  int lifts = 0;
  int stroke = 0;
  int shake = 0;
  int samples = 0;
};

// 指定の閾値でフィクスチャを流す
Counts runWith(const Thresholds &t, const std::string &name) {
  const std::vector<ImuSample> samples = loadFixture(name);
  MotionAnalyzer a;
  a.setThresholds(t);

  Counts c;
  c.samples = static_cast<int>(samples.size());
  for (const ImuSample &s : samples) {
    a.update(s);
    if (a.event() == MotionEvent::Tap) ++c.taps;
    if (a.event() == MotionEvent::Lift) ++c.lifts;
    if (a.activity() == Activity::Stroke) ++c.stroke;
    if (a.activity() == Activity::Shake) ++c.shake;
  }
  return c;
}

// 1 動作だけ測定を有効にした閾値を作る
Thresholds withCalibration(CalibTarget target, const std::string &fixture) {
  const Calibrator::Result r = calibrateFrom(target, fixture);
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.reason);

  CalibrationSet set;
  set.at(target) = r.calib;
  set.at(target).enabled = true;
  return pet::resolve(set);
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- 反映の仕組み ---

// 何も測っていなければ既定値のまま。
void test_empty_set_gives_defaults() {
  const Thresholds d;
  const Thresholds r = pet::resolve(CalibrationSet{});

  TEST_ASSERT_EQUAL_FLOAT(d.tapJerk, r.tapJerk);
  TEST_ASSERT_EQUAL_FLOAT(d.strokeMaxGyro, r.strokeMaxGyro);
  TEST_ASSERT_EQUAL_FLOAT(d.shakeMinLin, r.shakeMinLin);
  TEST_ASSERT_EQUAL_FLOAT(d.liftUpwardAccel, r.liftUpwardAccel);
}

// 測ってあっても無効なら反映されない。
// 「測ったが使わない」を表せることが、測り直しを不要にする。
void test_disabled_calibration_is_ignored() {
  const Calibrator::Result r = calibrateFrom(CalibTarget::Shake, "shake");
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.reason);

  CalibrationSet set;
  set.at(CalibTarget::Shake) = r.calib;
  set.at(CalibTarget::Shake).enabled = false;

  TEST_ASSERT_EQUAL_FLOAT(Thresholds{}.shakeMinLin,
                          pet::resolve(set).shakeMinLin);
}

// --- 測定できること ---

void test_stroke_calibration_succeeds() {
  const Calibrator::Result r = calibrateFrom(CalibTarget::Stroke, "stroke");
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.reason);
  TEST_ASSERT_TRUE(r.calib.hasData);
}

void test_shake_calibration_succeeds() {
  const Calibrator::Result r = calibrateFrom(CalibTarget::Shake, "shake");
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.reason);
}

void test_tap_calibration_succeeds() {
  const Calibrator::Result r = calibrateFrom(CalibTarget::Tap, "tap");
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.reason);
}

void test_lift_calibration_succeeds() {
  const Calibrator::Result r = calibrateFrom(CalibTarget::Lift, "lift");
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.reason);
}

// --- 壊れた測定を弾くこと ---
//
// 妥当性検査が無いと、失敗した測定が焼き付いて以後まともに反応しなくなる。

void test_rejects_stroke_recorded_without_touching() {
  const Calibrator::Result r = calibrateFrom(CalibTarget::Stroke, "idle");
  TEST_ASSERT_FALSE_MESSAGE(r.ok, "触れていない録音を撫でとして受け入れた");
}

void test_rejects_stroke_recorded_while_shaking() {
  const Calibrator::Result r = calibrateFrom(CalibTarget::Stroke, "shake");
  TEST_ASSERT_FALSE_MESSAGE(r.ok, "振りを撫でとして受け入れた");
}

void test_rejects_shake_recorded_while_still() {
  const Calibrator::Result r = calibrateFrom(CalibTarget::Shake, "idle");
  TEST_ASSERT_FALSE_MESSAGE(r.ok, "静止を振りとして受け入れた");
}

void test_rejects_tap_recorded_while_still() {
  const Calibrator::Result r = calibrateFrom(CalibTarget::Tap, "idle");
  TEST_ASSERT_FALSE_MESSAGE(r.ok, "静止をつつきとして受け入れた");
}

void test_rejects_lift_recorded_while_still() {
  const Calibrator::Result r = calibrateFrom(CalibTarget::Lift, "idle");
  TEST_ASSERT_FALSE_MESSAGE(r.ok, "静止を持ち上げとして受け入れた");
}

// --- 測定後も検出が壊れないこと ---
//
// ここが本丸。キャリブレーションの目的は精度を上げることだが、
// 下げてしまっては意味がない。

void test_stroke_calibration_still_detects_stroke() {
  const Thresholds t = withCalibration(CalibTarget::Stroke, "stroke");
  const Counts c = runWith(t, "stroke");

  TEST_ASSERT_TRUE_MESSAGE(c.stroke > c.samples / 2,
                           "自分の撫でで測ったのに撫でを検出できていない");
}

// 最重要。撫でを測り直しても、手に持って動かしているだけの状態を
// 撫でと誤判定してはいけない。
void test_stroke_calibration_still_rejects_being_carried() {
  const Thresholds t = withCalibration(CalibTarget::Stroke, "stroke");
  const Counts c = runWith(t, "walk");

  TEST_ASSERT_EQUAL_INT_MESSAGE(
      0, c.stroke, "撫でのキャリブレーション後に手持ちを撫でと誤判定している");
}

void test_shake_calibration_still_detects_shake() {
  const Thresholds t = withCalibration(CalibTarget::Shake, "shake");
  const Counts c = runWith(t, "shake");

  TEST_ASSERT_TRUE_MESSAGE(c.shake > c.samples / 2,
                           "振りのキャリブレーション後に振りを検出できていない");
}

void test_shake_calibration_does_not_make_carrying_a_shake() {
  const Thresholds t = withCalibration(CalibTarget::Shake, "shake");
  const Counts c = runWith(t, "walk");

  TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.shake, "手持ちを振りと誤判定している");
}

void test_tap_calibration_still_detects_taps() {
  const Thresholds t = withCalibration(CalibTarget::Tap, "tap");
  const Counts c = runWith(t, "tap");

  TEST_ASSERT_TRUE_MESSAGE(c.taps >= 3, "つつきを検出できていない");
}

void test_tap_calibration_does_not_fire_on_stroking() {
  const Thresholds t = withCalibration(CalibTarget::Tap, "tap");
  const Counts c = runWith(t, "stroke");

  TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.taps, "撫でをつつきと誤検出している");
}

void test_tap_calibration_does_not_fire_while_carried() {
  const Thresholds t = withCalibration(CalibTarget::Tap, "tap");
  const Counts c = runWith(t, "walk");

  TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.taps, "手持ちでつつきを誤検出している");
}

void test_lift_calibration_still_detects_lifts() {
  const Thresholds t = withCalibration(CalibTarget::Lift, "lift");
  const Counts c = runWith(t, "lift");

  TEST_ASSERT_TRUE_MESSAGE(c.lifts >= 2, "持ち上げを検出できていない");
}

void test_lift_calibration_does_not_fire_on_stroking() {
  const Thresholds t = withCalibration(CalibTarget::Lift, "lift");
  const Counts c = runWith(t, "stroke");

  TEST_ASSERT_EQUAL_INT_MESSAGE(0, c.lifts, "撫でを持ち上げと誤検出している");
}

// --- 値が範囲に収まること ---
//
// 想定外の録音から極端な値が出ても、検出が完全に死なないようにする。

void test_derived_values_stay_in_sane_ranges() {
  const char *fixtures[] = {"stroke", "shake", "tap", "lift", "walk", "idle"};
  const CalibTarget targets[] = {CalibTarget::Stroke, CalibTarget::Shake,
                                 CalibTarget::Tap, CalibTarget::Lift};

  for (CalibTarget target : targets) {
    for (const char *fixture : fixtures) {
      const Calibrator::Result r = calibrateFrom(target, fixture);
      if (!r.ok) {
        continue;  // 弾かれたものは反映されないので問題ない
      }

      CalibrationSet set;
      set.at(target) = r.calib;
      set.at(target).enabled = true;
      const Thresholds t = pet::resolve(set);

      TEST_ASSERT_TRUE(t.contactMinLin > 0.0f && t.contactMinLin < 0.2f);
      TEST_ASSERT_TRUE(t.strokeMaxGyro > 5.0f && t.strokeMaxGyro < 50.0f);
      TEST_ASSERT_TRUE(t.shakeMinLin > 0.1f && t.shakeMinLin < 1.0f);
      TEST_ASSERT_TRUE(t.tapJerk > 0.2f && t.tapJerk < 2.0f);
      TEST_ASSERT_TRUE(t.liftUpwardAccel > 0.1f && t.liftUpwardAccel < 1.0f);
    }
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_set_gives_defaults);
  RUN_TEST(test_disabled_calibration_is_ignored);
  RUN_TEST(test_stroke_calibration_succeeds);
  RUN_TEST(test_shake_calibration_succeeds);
  RUN_TEST(test_tap_calibration_succeeds);
  RUN_TEST(test_lift_calibration_succeeds);
  RUN_TEST(test_rejects_stroke_recorded_without_touching);
  RUN_TEST(test_rejects_stroke_recorded_while_shaking);
  RUN_TEST(test_rejects_shake_recorded_while_still);
  RUN_TEST(test_rejects_tap_recorded_while_still);
  RUN_TEST(test_rejects_lift_recorded_while_still);
  RUN_TEST(test_stroke_calibration_still_detects_stroke);
  RUN_TEST(test_stroke_calibration_still_rejects_being_carried);
  RUN_TEST(test_shake_calibration_still_detects_shake);
  RUN_TEST(test_shake_calibration_does_not_make_carrying_a_shake);
  RUN_TEST(test_tap_calibration_still_detects_taps);
  RUN_TEST(test_tap_calibration_does_not_fire_on_stroking);
  RUN_TEST(test_tap_calibration_does_not_fire_while_carried);
  RUN_TEST(test_lift_calibration_still_detects_lifts);
  RUN_TEST(test_lift_calibration_does_not_fire_on_stroking);
  RUN_TEST(test_derived_values_stay_in_sane_ranges);
  return UNITY_END();
}
