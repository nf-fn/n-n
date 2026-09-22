// MotionAnalyzer の姿勢算出テスト (フェーズ 1)
//
// 静止時の加速度ベクトルは「世界の上方向」を機体座標で表したものになる。
// 軸は +X 右 / +Y 上 / +Z 画面の外向き。

#include <unity.h>

#include <cmath>

#include "MotionAnalyzer.h"
#include "Types.h"

using pet::ImuSample;
using pet::MotionAnalyzer;
using pet::Posture;

namespace {

constexpr float kPi = 3.14159265358979f;

// テスト全体で単調増加させる時計 [ms]。巻き戻すと平滑化の前提が崩れる。
uint32_t g_clock = 0;

// 一定の加速度を与え続けて平滑化を収束させる
Posture settle(MotionAnalyzer &analyzer, float ax, float ay, float az,
               int samples = 300) {
  for (int i = 0; i < samples; ++i) {
    ImuSample s;
    s.ax = ax;
    s.ay = ay;
    s.az = az;
    g_clock += 10;  // 100Hz
    s.tMs = g_clock;
    analyzer.update(s);
  }
  return analyzer.posture();
}

}  // namespace

void setUp() {}
void tearDown() {}

// 画面を上に向けて水平。下り坂は無く、目玉は中央に留まる。
void test_flat_face_up_has_no_downhill() {
  MotionAnalyzer analyzer;
  const Posture p = settle(analyzer, -0.012f, -0.008f, 0.993f);

  TEST_ASSERT_TRUE(p.faceUp);
  TEST_ASSERT_FALSE(p.faceDown);
  TEST_ASSERT_FALSE(p.inverted);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, p.downX);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, p.downY);
}

// 画面を手前に立てて垂直。下り坂は画面の真下を向く。
void test_upright_downhill_points_down() {
  MotionAnalyzer analyzer;
  const Posture p = settle(analyzer, -0.003f, 0.998f, 0.026f);

  TEST_ASSERT_FALSE(p.faceUp);
  TEST_ASSERT_FALSE(p.faceDown);
  TEST_ASSERT_FALSE(p.inverted);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, p.downX);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, -1.0f, p.downY);
}

// 垂直姿勢から上端を右へ傾けると、下り坂は右下を向く。
// フェーズ 0 の実測値 (48.5 度) をそのまま使う。
void test_tilt_right_downhill_points_bottom_right() {
  MotionAnalyzer analyzer;
  const Posture p = settle(analyzer, -0.748f, 0.661f, 0.013f);

  TEST_ASSERT_TRUE(p.downX > 0.5f);   // 右
  TEST_ASSERT_TRUE(p.downY < -0.5f);  // かつ下
}

// 上端が下を向くと上下逆さ。顔は見えているので反応させたい。
void test_inverted_when_top_edge_points_down() {
  MotionAnalyzer analyzer;
  const Posture p = settle(analyzer, 0.0f, -0.99f, 0.05f);

  TEST_ASSERT_TRUE(p.inverted);
  TEST_ASSERT_FALSE(p.faceDown);
  TEST_ASSERT_TRUE(p.downY > 0.5f);  // 下り坂は画面上方向へ
}

// 画面を下に伏せた状態。顔が見えないので inverted とは区別する。
void test_face_down_is_distinct_from_inverted() {
  MotionAnalyzer analyzer;
  const Posture p = settle(analyzer, -0.013f, 0.035f, -1.008f);

  TEST_ASSERT_TRUE(p.faceDown);
  TEST_ASSERT_FALSE(p.faceUp);
  TEST_ASSERT_FALSE(p.inverted);
}

// 下り坂ベクトルの長さは 0..1 に収まる。目玉が顔からはみ出さない前提。
void test_downhill_magnitude_never_exceeds_one() {
  MotionAnalyzer analyzer;
  const Posture p = settle(analyzer, -0.003f, 0.998f, 0.026f);

  const float mag = std::sqrt(p.downX * p.downX + p.downY * p.downY);
  TEST_ASSERT_TRUE(mag <= 1.001f);
}

// 激しく振っている最中も姿勢が暴れないこと。
// 生の加速度をそのまま使うと顔が壊れるため、平滑化が効いている必要がある。
void test_violent_shaking_does_not_destroy_posture() {
  MotionAnalyzer analyzer;
  settle(analyzer, 0.0f, 1.0f, 0.0f);  // 垂直姿勢で落ち着かせる
  const Posture before = analyzer.posture();

  // 重力の周りに ±3g で暴れる信号を 1 秒ぶん (100 サンプル) 入れる
  for (int i = 0; i < 100; ++i) {
    const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
    ImuSample s;
    s.ax = 3.0f * sign;
    s.ay = 1.0f + 3.0f * sign;
    s.az = 3.0f * sign;
    g_clock += 10;
    s.tMs = g_clock;
    analyzer.update(s);
  }
  const Posture after = analyzer.posture();

  TEST_ASSERT_FLOAT_WITHIN(0.25f, before.downX, after.downX);
  TEST_ASSERT_FLOAT_WITHIN(0.25f, before.downY, after.downY);
}

// 傾けたときは追従すること。平滑化が強すぎて反応しないのも困る。
void test_posture_follows_a_real_tilt() {
  MotionAnalyzer analyzer;
  settle(analyzer, 0.0f, 1.0f, 0.0f);

  // 0.5 秒ぶん (50 サンプル) 傾けた姿勢を入れれば追従してほしい
  for (int i = 0; i < 50; ++i) {
    ImuSample s;
    s.ax = -std::sin(45.0f * kPi / 180.0f);
    s.ay = std::cos(45.0f * kPi / 180.0f);
    s.az = 0.0f;
    g_clock += 10;
    s.tMs = g_clock;
    analyzer.update(s);
  }

  TEST_ASSERT_TRUE(analyzer.posture().downX > 0.4f);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_flat_face_up_has_no_downhill);
  RUN_TEST(test_upright_downhill_points_down);
  RUN_TEST(test_tilt_right_downhill_points_bottom_right);
  RUN_TEST(test_inverted_when_top_edge_points_down);
  RUN_TEST(test_face_down_is_distinct_from_inverted);
  RUN_TEST(test_downhill_magnitude_never_exceeds_one);
  RUN_TEST(test_violent_shaking_does_not_destroy_posture);
  RUN_TEST(test_posture_follows_a_real_tilt);
  return UNITY_END();
}
