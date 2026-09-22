// FaceComposer のテスト (フェーズ 1)
//
// フェーズ 1 の責務は「姿勢に応じて目玉を転がす」と「まばたき」の 2 つだけ。
// 気分による表情はフェーズ 3 で足す。

#include <unity.h>

#include <cmath>

#include "FaceComposer.h"
#include "Types.h"

using pet::FaceComposer;
using pet::FaceParams;
using pet::Posture;

namespace {

// 「世界の上」方向から Posture を組み立てる。MotionAnalyzer と同じ規則。
Posture postureFromUp(float ux, float uy, float uz) {
  const float mag = std::sqrt(ux * ux + uy * uy + uz * uz);
  Posture p;
  p.upX = ux / mag;
  p.upY = uy / mag;
  p.upZ = uz / mag;
  p.downX = -p.upX;
  p.downY = -p.upY;
  p.faceUp = p.upZ > 0.7f;
  p.faceDown = p.upZ < -0.7f;
  p.inverted = p.upY < -0.5f;
  return p;
}

// まばたきを挟まない時刻で 1 枚だけ取る
FaceParams composeAt(FaceComposer &composer, const Posture &p, uint32_t tMs) {
  return composer.compose(p, tMs);
}

}  // namespace

void setUp() {}
void tearDown() {}

// 画面を上に向けて水平なら目玉は中央。
void test_flat_keeps_eyes_centered() {
  FaceComposer composer;
  const FaceParams f = composeAt(composer, postureFromUp(0.0f, 0.0f, 1.0f), 0);

  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.0f, f.eyeOffsetX);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.0f, f.eyeOffsetY);
}

// 垂直に立てると目玉は下に落ちる。
void test_upright_drops_eyes_down() {
  FaceComposer composer;
  const FaceParams f = composeAt(composer, postureFromUp(0.0f, 1.0f, 0.0f), 0);

  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, f.eyeOffsetX);
  TEST_ASSERT_TRUE(f.eyeOffsetY < -0.8f);
}

// 上端を右へ傾けると目玉は右下へ転がる。
void test_tilt_right_rolls_eyes_bottom_right() {
  FaceComposer composer;
  const FaceParams f =
      composeAt(composer, postureFromUp(-0.749f, 0.662f, 0.0f), 0);

  TEST_ASSERT_TRUE(f.eyeOffsetX > 0.5f);
  TEST_ASSERT_TRUE(f.eyeOffsetY < -0.5f);
}

// 目玉は必ず -1..1 に収まる。顔の輪郭からはみ出させない。
void test_eye_offset_is_always_clamped() {
  FaceComposer composer;
  const float angles[] = {0.0f, 30.0f, 45.0f, 90.0f, 135.0f, 180.0f, 270.0f};
  for (float deg : angles) {
    const float rad = deg * 3.14159265f / 180.0f;
    const Posture p = postureFromUp(-std::sin(rad), std::cos(rad), 0.0f);
    const FaceParams f = composeAt(composer, p, 0);

    const float mag =
        std::sqrt(f.eyeOffsetX * f.eyeOffsetX + f.eyeOffsetY * f.eyeOffsetY);
    TEST_ASSERT_TRUE(mag <= 1.001f);
  }
}

// 傾けると顔は世界の上を向こうとして少しだけ傾く (完全には戻らない)。
void test_face_tilts_partially_against_device_rotation() {
  FaceComposer composer;
  // 上端を右へ 48.6 度 = 時計回り。顔は反時計回り (正) に戻ろうとする。
  const FaceParams f =
      composeAt(composer, postureFromUp(-0.749f, 0.662f, 0.0f), 0);

  TEST_ASSERT_TRUE(f.faceTiltRad > 0.0f);
  // 完全に水平を保つと「水準器」になって生き物に見えないので、
  // 傾きの半分より小さく戻すに留める。
  TEST_ASSERT_TRUE(f.faceTiltRad < 0.848f * 0.5f);
}

// 水平なら顔も傾かない。
void test_flat_has_no_face_tilt() {
  FaceComposer composer;
  const FaceParams f = composeAt(composer, postureFromUp(0.0f, 0.0f, 1.0f), 0);

  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.0f, f.faceTiltRad);
}

// 10 秒のあいだに必ずまばたきする。
void test_blinks_within_ten_seconds() {
  FaceComposer composer;
  const Posture p = postureFromUp(0.0f, 0.0f, 1.0f);

  bool sawClosed = false;
  for (uint32_t t = 0; t < 10000; t += 20) {
    if (composer.compose(p, t).eyeOpen < 0.3f) {
      sawClosed = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(sawClosed);
}

// まばたきは一瞬で終わり、目は開いた状態に戻る。
void test_blink_is_brief_and_eyes_reopen() {
  FaceComposer composer;
  const Posture p = postureFromUp(0.0f, 0.0f, 1.0f);

  int closedFrames = 0;
  int totalFrames = 0;
  for (uint32_t t = 0; t < 20000; t += 20) {
    if (composer.compose(p, t).eyeOpen < 0.3f) {
      ++closedFrames;
    }
    ++totalFrames;
  }

  TEST_ASSERT_TRUE(closedFrames > 0);
  // 閉じている時間は全体の 1 割未満。これを超えると眠そうに見える。
  TEST_ASSERT_TRUE(closedFrames * 10 < totalFrames);
}

// まばたきの間隔は一定ではない。機械的だと生き物に見えない。
void test_blink_intervals_vary() {
  FaceComposer composer;
  const Posture p = postureFromUp(0.0f, 0.0f, 1.0f);

  uint32_t starts[8] = {0};
  int count = 0;
  bool wasClosed = false;
  for (uint32_t t = 0; t < 60000 && count < 8; t += 20) {
    const bool closed = composer.compose(p, t).eyeOpen < 0.3f;
    if (closed && !wasClosed) {
      starts[count++] = t;
    }
    wasClosed = closed;
  }

  TEST_ASSERT_TRUE(count >= 4);

  bool sawDifferentInterval = false;
  const uint32_t first = starts[1] - starts[0];
  for (int i = 2; i < count; ++i) {
    if (starts[i] - starts[i - 1] != first) {
      sawDifferentInterval = true;
    }
  }
  TEST_ASSERT_TRUE(sawDifferentInterval);
}

// この顔は口を持たない。素の状態では眉も出さない。
// 感情はフェーズ 3 で載せるので、フェーズ 1 では常に中立であること。
void test_neutral_face_has_no_brows() {
  FaceComposer composer;
  const FaceParams f =
      composeAt(composer, postureFromUp(0.0f, 0.0f, 1.0f), 0);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, f.browAngle);
}

// まばたきは笑いではない。閉じた目が上向きの弧になってはいけない。
void test_blink_is_not_a_smile() {
  FaceComposer composer;
  const Posture p = postureFromUp(0.0f, 0.0f, 1.0f);

  for (uint32_t t = 0; t < 20000; t += 20) {
    const FaceParams f = composer.compose(p, t);
    if (f.eyeOpen < 0.3f) {
      TEST_ASSERT_TRUE(f.eyeArch <= 0.0f);
    }
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_flat_keeps_eyes_centered);
  RUN_TEST(test_upright_drops_eyes_down);
  RUN_TEST(test_tilt_right_rolls_eyes_bottom_right);
  RUN_TEST(test_eye_offset_is_always_clamped);
  RUN_TEST(test_face_tilts_partially_against_device_rotation);
  RUN_TEST(test_flat_has_no_face_tilt);
  RUN_TEST(test_blinks_within_ten_seconds);
  RUN_TEST(test_blink_is_brief_and_eyes_reopen);
  RUN_TEST(test_blink_intervals_vary);
  RUN_TEST(test_neutral_face_has_no_brows);
  RUN_TEST(test_blink_is_not_a_smile);
  return UNITY_END();
}
