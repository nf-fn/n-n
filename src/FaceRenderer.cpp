#include "FaceRenderer.h"

#include <cmath>

namespace pet {
namespace {

constexpr int kSize = 128;
constexpr int kCenter = kSize / 2;

// 顔の配置 (顔座標: 原点 = 画面中心、+X 右 / +Y 上、単位 = ピクセル)
constexpr int kEyeSpacing = 27;  // 中心から左右の目までの距離
constexpr int kEyeHeight = 12;   // 目の高さ (中心より上)
constexpr int kEyeRadius = 20;   // 白目の半径
constexpr int kPupilRadius = 9;  // 黒目の半径
constexpr int kPupilTravel = kEyeRadius - kPupilRadius - 2;

constexpr int kMouthHeight = -32;  // 口の高さ (中心より下)
constexpr int kMouthHalfWidth = 18;
constexpr int kMouthMaxCurve = 10;

// 顔座標をスプライトのピクセル座標へ。画面の Y は下向きなので反転する。
inline int px(float x) { return kCenter + static_cast<int>(std::lround(x)); }
inline int py(float y) { return kCenter - static_cast<int>(std::lround(y)); }

}  // namespace

bool FaceRenderer::begin() {
  M5.Display.setRotation(0);

  skin_ = M5.Display.color565(242, 213, 179);
  ink_ = M5.Display.color565(42, 33, 24);
  white_ = M5.Display.color565(255, 252, 245);

  frame_.setColorDepth(16);
  face_.setColorDepth(16);

  if (!frame_.createSprite(kSize, kSize) || !face_.createSprite(kSize, kSize)) {
    return false;
  }

  // 回転の中心を顔の中心に合わせる
  face_.setPivot(kCenter, kCenter);

  ready_ = true;
  return true;
}

void FaceRenderer::drawFaceUpright(const FaceParams &params) {
  face_.fillSprite(skin_);

  // --- 目 ---
  // eyeOpen で白目を縦につぶす。完全に閉じたら線を引く。
  const float open = params.eyeOpen < 0.0f   ? 0.0f
                     : params.eyeOpen > 1.0f ? 1.0f
                                             : params.eyeOpen;
  const int eyeRy = static_cast<int>(std::lround(kEyeRadius * open));

  for (int side = -1; side <= 1; side += 2) {
    const float cx = static_cast<float>(side * kEyeSpacing);
    const float cy = static_cast<float>(kEyeHeight);

    if (eyeRy < 2) {
      // 閉じた目は一本線
      face_.drawFastHLine(px(cx - kEyeRadius), py(cy), kEyeRadius * 2, ink_);
      continue;
    }

    face_.fillEllipse(px(cx), py(cy), kEyeRadius, eyeRy, white_);

    // 黒目は下り坂の方向へ寄る。白目の中に収まるよう縦を抑える。
    const float ox = params.eyeOffsetX * kPupilTravel;
    const float oy = params.eyeOffsetY * kPupilTravel;
    const int pupilRy = kPupilRadius < eyeRy ? kPupilRadius : eyeRy;

    // 目を細めているときは黒目も上下にはみ出さない位置まで引き戻す
    const float maxOy = static_cast<float>(eyeRy - pupilRy);
    const float clampedOy = oy > maxOy ? maxOy : (oy < -maxOy ? -maxOy : oy);

    face_.fillEllipse(px(cx + ox), py(cy + clampedOy), kPupilRadius, pupilRy,
                      ink_);
  }

  // --- 口 ---
  // mouthCurve が 0 なら一文字、正なら笑い、負ならへの字。
  const float curve = params.mouthCurve * kMouthMaxCurve;
  int prevX = px(static_cast<float>(-kMouthHalfWidth));
  int prevY = py(static_cast<float>(kMouthHeight));
  for (int i = 1; i <= 12; ++i) {
    const float t = -1.0f + 2.0f * static_cast<float>(i) / 12.0f;
    const float x = t * kMouthHalfWidth;
    const float y = kMouthHeight + curve * (1.0f - t * t);
    const int nx = px(x);
    const int ny = py(y);
    // 線を 2 本重ねて太くする
    face_.drawLine(prevX, prevY, nx, ny, ink_);
    face_.drawLine(prevX, prevY + 1, nx, ny + 1, ink_);
    prevX = nx;
    prevY = ny;
  }
}

void FaceRenderer::draw(const FaceParams &params) {
  if (!ready_) {
    return;
  }

  drawFaceUpright(params);

  // 顔を傾けてフレームへ合成する。
  // 肌色を透過色に指定しているので、フレーム側の肌色と継ぎ目なく繋がる。
  frame_.fillSprite(skin_);

  const float degrees = params.faceTiltRad * 180.0f / 3.14159265f;
  // LovyanGFX の回転は時計回りが正。顔座標は反時計回りが正なので符号を返す。
  face_.pushRotateZoom(&frame_, kCenter, kCenter, -degrees, 1.0f, 1.0f, skin_);

  frame_.pushSprite(0, 0);
}

}  // namespace pet
