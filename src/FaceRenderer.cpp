#include "FaceRenderer.h"

#include <cmath>

namespace pet {
namespace {

constexpr int kSize = 128;
constexpr int kCenter = kSize / 2;

// 顔座標をスプライトのピクセル座標へ。画面の Y は下向きなので反転する。
inline int px(float x) { return kCenter + static_cast<int>(std::lround(x)); }
inline int py(float y) { return kCenter - static_cast<int>(std::lround(y)); }

inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// --- 寸法 (顔座標: 原点 = 画面中心、+X 右 / +Y 上、単位 = ピクセル) ---

// 頭は横に長い角丸の長方形。上下を詰めて平たくし、四隅の丸みが見える位置に
// 収める。顔パッチは持たず、頭は一色。
constexpr int kHeadLeft = -62;
constexpr int kHeadRight = 62;
constexpr int kHeadTop = 26;
constexpr int kHeadBottom = -60;
constexpr int kHeadCorner = 34;

// 目。白目の中に虹彩があり、虹彩の下側に水色の三日月が入る。
// 頭の幅 124 に対して白目の直径 34。目と鼻はやや下寄りに置く。
constexpr int kEyeSpacing = 33;
constexpr int kEyeY = -9;
constexpr int kScleraRx = 17;
constexpr int kScleraRy = 18;
constexpr int kIrisR = 12;

// 虹彩の下に覗く水色。濃い円を少し上へずらして三日月を作る。
constexpr float kGlowShift = 2.5f;

// 視線の可動量。虹彩が白目の中を動く。半径の差 (5) が上限。
constexpr float kIrisTravel = 4.0f;

// 鼻は横長の楕円。目の中心を通る線より少し下に置く。
constexpr int kNoseRx = 6;
constexpr int kNoseRy = 4;
constexpr int kNoseY = -18;

// 眉。素の顔には無く、怒り・困りのときだけ現れる。
// 目は y = -9 を中心に ±18 まで広がるので、その上に出す。
constexpr int kBrowY = 17;
constexpr int kBrowHalfWidth = 10;
constexpr float kBrowInnerDrop = 6.0f;  // 怒ったとき内側が下がる量
constexpr float kBrowOuterLift = 2.5f;  // 同じく外側が上がる量

}  // namespace

bool FaceRenderer::begin() {
  M5.Display.setRotation(0);

  bg_ = M5.Display.color565(250, 249, 247);
  head_ = M5.Display.color565(216, 211, 205);
  white_ = M5.Display.color565(255, 255, 255);
  irisDark_ = M5.Display.color565(22, 34, 60);
  irisGlow_ = M5.Display.color565(63, 198, 232);
  nose_ = M5.Display.color565(150, 144, 137);

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

void FaceRenderer::drawEye(int side, const FaceParams &params, float open) {
  const float cx = static_cast<float>(side * kEyeSpacing);
  const float cy = static_cast<float>(kEyeY);

  const int socketRy = static_cast<int>(std::lround(kScleraRy * open));
  if (socketRy < 3) {
    // 閉じた目の弧。向きで意味が変わるので eyeArch から決める。
    //   eyeArch  0 → 中央がわずかに下がる ∪ (まばたき)
    //   eyeArch +1 → 中央が持ち上がる ∩ (笑い)
    //   eyeArch -1 → 深い ∪ (眠い)
    const float arch = -3.0f + params.eyeArch * 7.0f;

    int prevX = px(cx - kScleraRx);
    int prevY = py(cy);
    for (int i = 1; i <= 10; ++i) {
      const float t = -1.0f + 2.0f * static_cast<float>(i) / 10.0f;
      const float x = cx + t * kScleraRx;
      const float y = cy + arch * (1.0f - t * t);
      const int nx = px(x);
      const int ny = py(y);
      // 3 本重ねて太くする。細いと笑っているように見えない。
      face_.drawLine(prevX, prevY, nx, ny, irisDark_);
      face_.drawLine(prevX, prevY + 1, nx, ny + 1, irisDark_);
      face_.drawLine(prevX, prevY + 2, nx, ny + 2, irisDark_);
      prevX = nx;
      prevY = ny;
    }
    return;
  }

  face_.fillEllipse(px(cx), py(cy), kScleraRx, socketRy, white_);

  // 虹彩。興奮すると瞳が開く。可動量も大きさも白目からはみ出さないよう抑える。
  const int irisR = static_cast<int>(
      std::lround(kIrisR * clampf(params.irisScale, 0.5f, 1.6f)));
  const int irisRx = irisR < kScleraRx - 1 ? irisR : kScleraRx - 1;
  const int irisRy = irisR < socketRy ? irisR : socketRy;

  const float maxOx = static_cast<float>(kScleraRx - irisRx);
  const float maxOy = static_cast<float>(socketRy - irisRy);
  const float ox = clampf(params.eyeOffsetX * kIrisTravel, -maxOx, maxOx);
  const float oy = clampf(params.eyeOffsetY * kIrisTravel, -maxOy, maxOy);

  const float ix = cx + ox;
  const float iy = cy + oy;

  // 虹彩の下に水色を覗かせる。水色で塗ってから、濃い円を少し上にずらして
  // 重ねると、下側だけが三日月として残る。
  face_.fillEllipse(px(ix), py(iy), irisRx, irisRy, irisGlow_);
  const int innerRy = irisRy - 1 > 1 ? irisRy - 1 : 1;
  face_.fillEllipse(px(ix), py(iy + kGlowShift * open), irisRx - 1, innerRy,
                    irisDark_);

  // ハイライト。生気のほとんどはここで決まる。
  if (irisRy >= 6) {
    face_.fillEllipse(px(ix - irisRx * 0.38f), py(iy + irisRy * 0.36f), 4, 4,
                      white_);
    face_.fillEllipse(px(ix + irisRx * 0.46f), py(iy - irisRy * 0.30f), 2, 2,
                      white_);
  }
}

// 眉。browAngle が 0 のときは描かない。
// 素の顔に眉が無いぶん、出たときの印象が強くなる。
void FaceRenderer::drawBrow(int side, const FaceParams &params) {
  if (params.browAngle > -0.05f && params.browAngle < 0.05f) {
    return;
  }

  const float a = clampf(params.browAngle, -1.0f, 1.0f);

  // 内側 = 顔の中心寄り。怒ると内側が下がり、困ると内側が上がる。
  const float innerX =
      static_cast<float>(side * (kEyeSpacing - kBrowHalfWidth));
  const float outerX =
      static_cast<float>(side * (kEyeSpacing + kBrowHalfWidth));
  const float innerY = kBrowY - a * kBrowInnerDrop;
  const float outerY = kBrowY + a * kBrowOuterLift;

  for (int d = 0; d < 3; ++d) {
    face_.drawLine(px(innerX), py(innerY) + d, px(outerX), py(outerY) + d,
                   irisDark_);
  }
}

void FaceRenderer::drawFace(const FaceParams &params, float open) {
  // 背景 (透過色) の上に、角丸の頭を 1 色で置く
  face_.fillSprite(bg_);
  face_.fillRoundRect(px(kHeadLeft), py(kHeadTop), kHeadRight - kHeadLeft,
                      kHeadTop - kHeadBottom, kHeadCorner, head_);

  drawEye(-1, params, open);
  drawEye(1, params, open);

  // 鼻。口は持たない。
  face_.fillEllipse(px(0.0f), py(kNoseY), kNoseRx, kNoseRy, nose_);

  drawBrow(-1, params);
  drawBrow(1, params);
}

void FaceRenderer::draw(const FaceParams &params) {
  if (!ready_) {
    return;
  }

  drawFace(params, clampf(params.eyeOpen, 0.0f, 1.0f));

  // 顔を傾けてフレームへ合成する。
  // 背景色を透過色に指定しているので、フレーム側の背景と継ぎ目なく繋がる。
  frame_.fillSprite(bg_);

  const float degrees = params.faceTiltRad * 180.0f / 3.14159265f;
  // LovyanGFX の回転は時計回りが正。顔座標は反時計回りが正なので符号を返す。
  face_.pushRotateZoom(&frame_, kCenter, kCenter, -degrees, 1.0f, 1.0f, bg_);

  frame_.pushSprite(0, 0);
}

}  // namespace pet
