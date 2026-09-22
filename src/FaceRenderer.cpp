#include "FaceRenderer.h"

#include <cmath>

namespace pet {

// 実機で見比べるための案。5 は参考画像から起こしたもので、これを既定とする。
//
// かわいさに効く要素を意図的に振っている:
//   目の大きさ / 目の高さ / 白目の有無 / 瞳の光 / 口の小ささ / ほっぺ
const FaceStyle kStyles[kStyleCount] = {
    // 0: 最初に作ったもの。比較の基準。
    //    目が中心より上にあり、口が横に広く平ら。
    {"CLASSIC", FaceKind::Simple, 242, 213, 179,
     /*spacing*/ 27, /*height*/ 12, /*rx*/ 20, /*ry*/ 20,
     /*sclera*/ true, /*pupilR*/ 9, /*travel*/ 9,
     /*catchlights*/ 0, /*catchR*/ 0,
     /*mouthY*/ -32, /*mouthHW*/ 18, /*mouthCurve*/ 10, /*rest*/ 0.0f,
     /*blush*/ false},

    // 1: 赤ちゃんの比率。目を大きく、低く、離す。口を小さくして下げる。
    {"BABY", FaceKind::Simple, 245, 222, 196,
     /*spacing*/ 30, /*height*/ -2, /*rx*/ 25, /*ry*/ 25,
     /*sclera*/ true, /*pupilR*/ 12, /*travel*/ 10,
     /*catchlights*/ 2, /*catchR*/ 4,
     /*mouthY*/ -44, /*mouthHW*/ 9, /*mouthCurve*/ 6, /*rest*/ 0.35f,
     /*blush*/ false},

    // 2: 白目を無くし、真っ黒な目に大きな光。目全体が動く。
    {"SOLID", FaceKind::Simple, 250, 236, 214,
     /*spacing*/ 29, /*height*/ -4, /*rx*/ 21, /*ry*/ 24,
     /*sclera*/ false, /*pupilR*/ 0, /*travel*/ 7,
     /*catchlights*/ 2, /*catchR*/ 6,
     /*mouthY*/ -46, /*mouthHW*/ 8, /*mouthCurve*/ 6, /*rest*/ 0.3f,
     /*blush*/ true},

    // 3: 点目。造作を減らして、ほっぺで甘さを出す。
    {"DOT", FaceKind::Simple, 252, 240, 222,
     /*spacing*/ 33, /*height*/ -2, /*rx*/ 10, /*ry*/ 11,
     /*sclera*/ false, /*pupilR*/ 0, /*travel*/ 5,
     /*catchlights*/ 1, /*catchR*/ 3,
     /*mouthY*/ -34, /*mouthHW*/ 7, /*mouthCurve*/ 5, /*rest*/ 0.5f,
     /*blush*/ true},

    // 4: 目をぎりぎりまで大きくした最大値。振り切るとどう見えるかの確認。
    {"HUGE", FaceKind::Simple, 246, 226, 202,
     /*spacing*/ 31, /*height*/ 0, /*rx*/ 29, /*ry*/ 30,
     /*sclera*/ true, /*pupilR*/ 15, /*travel*/ 11,
     /*catchlights*/ 2, /*catchR*/ 5,
     /*mouthY*/ -50, /*mouthHW*/ 7, /*mouthCurve*/ 5, /*rest*/ 0.3f,
     /*blush*/ true},

    // 5: 参考画像から起こした案。淡いグレーの角丸の頭、白目の中の濃紺の虹彩、
    //    その下に覗く水色、大きな白ハイライト、小さなグレーの鼻。
    //    口も耳も顔パッチも持たない。
    //    造作の構成が他と違うため、寸法は drawPlushFace 側の定数で持つ。
    {"PLUSH", FaceKind::Plush, 250, 249, 247,
     /*spacing*/ 0, /*height*/ 0, /*rx*/ 0, /*ry*/ 0,
     /*sclera*/ false, /*pupilR*/ 0, /*travel*/ 0,
     /*catchlights*/ 0, /*catchR*/ 0,
     /*mouthY*/ 0, /*mouthHW*/ 0, /*mouthCurve*/ 0, /*rest*/ 0.0f,
     /*blush*/ false},
};

namespace {

constexpr int kSize = 128;
constexpr int kCenter = kSize / 2;

// 顔座標をスプライトのピクセル座標へ。画面の Y は下向きなので反転する。
inline int px(float x) { return kCenter + static_cast<int>(std::lround(x)); }
inline int py(float y) { return kCenter - static_cast<int>(std::lround(y)); }

inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// --- Plush の寸法 (顔座標) ---
//
// 参考画像の比率を 128x128 に落としたもの。画面いっぱいに顔が来るよう、
// 頭は画面より少し大きく取って端を切らせている。
namespace plush {

// 頭は横に長い角丸の長方形。左右は画面際まで広げ、上を下げて平たくする。
// 上下を詰めて平たくし、四隅の丸みが見える位置に収める。
// 顔パッチは持たず、頭は一色。
constexpr int kHeadLeft = -62;
constexpr int kHeadRight = 62;
constexpr int kHeadTop = 26;
constexpr int kHeadBottom = -60;
constexpr int kHeadCorner = 34;

// 目。白目の中に虹彩があり、虹彩の下側に水色の三日月が入る。
//
// 参考画像では頭の幅に対する目の幅が 0.23 程度。頭幅 124 に対して
// 白目の直径 34 なので 0.27 で、そこに寄せてある。
// 目を小さくしたぶん頭を平たくできた。目と鼻はやや下寄りに置く。
constexpr int kEyeSpacing = 33;
constexpr int kEyeY = -9;
constexpr int kScleraRx = 17;
constexpr int kScleraRy = 18;
constexpr int kIrisR = 12;

// 虹彩の下に覗く水色。濃い円を少し上へずらして三日月を作る。
constexpr float kGlowShift = 2.5f;

// 視線の可動量。虹彩が白目の中を動く。半径の差 (5) が上限。
constexpr float kIrisTravel = 4.0f;

// 鼻は横長の楕円
constexpr int kNoseRx = 6;
constexpr int kNoseRy = 4;
// 目の中心を通る線より少し下に置く。
constexpr int kNoseY = -18;

// 眉。素の顔には無く、怒り・困りのときだけ現れる。
// 目は y = -9 を中心に ±18 まで広がるので、その上に出す。
constexpr int kBrowY = 17;
constexpr int kBrowHalfWidth = 10;
constexpr float kBrowInnerDrop = 6.0f;  // 怒ったとき内側が下がる量
constexpr float kBrowOuterLift = 2.5f;  // 同じく外側が上がる量

}  // namespace plush

}  // namespace

bool FaceRenderer::begin() {
  M5.Display.setRotation(0);

  ink_ = M5.Display.color565(42, 33, 24);
  white_ = M5.Display.color565(255, 252, 245);
  blush_ = M5.Display.color565(244, 160, 150);

  fur_ = M5.Display.color565(216, 211, 205);    // 頭のグレー
  irisDark_ = M5.Display.color565(22, 34, 60);  // 虹彩の濃紺
  irisGlow_ = M5.Display.color565(63, 198, 232);  // 虹彩の下に覗く水色
  nose_ = M5.Display.color565(150, 144, 137);

  frame_.setColorDepth(16);
  face_.setColorDepth(16);

  if (!frame_.createSprite(kSize, kSize) || !face_.createSprite(kSize, kSize)) {
    return false;
  }

  // 回転の中心を顔の中心に合わせる
  face_.setPivot(kCenter, kCenter);

  setStyle(kDefaultStyle);
  ready_ = true;
  return true;
}

void FaceRenderer::setStyle(int index) {
  if (index < 0 || index >= kStyleCount) {
    return;
  }
  styleIndex_ = index;
  const FaceStyle &s = kStyles[index];
  skin_ = M5.Display.color565(s.skinR, s.skinG, s.skinB);
}

void FaceRenderer::drawEye(int side, const FaceParams &params, float open) {
  const FaceStyle &s = kStyles[styleIndex_];

  const float cx = static_cast<float>(side * s.eyeSpacing);
  const float cy = static_cast<float>(s.eyeHeight);

  const int ry = static_cast<int>(std::lround(s.eyeRy * open));
  if (ry < 2) {
    // 閉じた目は一本線
    face_.drawFastHLine(px(cx - s.eyeRx), py(cy), s.eyeRx * 2, ink_);
    face_.drawFastHLine(px(cx - s.eyeRx), py(cy) + 1, s.eyeRx * 2, ink_);
    return;
  }

  const float ox = params.eyeOffsetX * s.travel;
  const float oy = params.eyeOffsetY * s.travel;

  // 瞳の中心。白目があるなら白目の中を動き、無いなら目そのものが動く。
  float pupilCx = cx;
  float pupilCy = cy;
  int pupilRx = 0;
  int pupilRy = 0;

  if (s.hasSclera) {
    face_.fillEllipse(px(cx), py(cy), s.eyeRx, ry, white_);

    pupilRx = s.pupilR;
    pupilRy = s.pupilR < ry ? s.pupilR : ry;

    // 目を細めているとき、瞳が白目から上下にはみ出さないよう引き戻す
    const float maxOy = static_cast<float>(ry - pupilRy);
    pupilCx = cx + ox;
    pupilCy = cy + clampf(oy, -maxOy, maxOy);
  } else {
    pupilRx = s.eyeRx;
    pupilRy = ry;
    pupilCx = cx + ox;
    pupilCy = cy + oy;
  }

  face_.fillEllipse(px(pupilCx), py(pupilCy), pupilRx, pupilRy, ink_);

  // 瞳の光。生気はほぼこれで決まるので、瞳と一緒に動かす。
  if (s.catchlights >= 1 && pupilRy > s.catchlightR + 1) {
    const float hx = pupilCx - pupilRx * 0.35f;
    const float hy = pupilCy + pupilRy * 0.38f;
    face_.fillCircle(px(hx), py(hy), s.catchlightR, white_);
  }
  if (s.catchlights >= 2 && pupilRy > s.catchlightR + 1) {
    const float hx = pupilCx + pupilRx * 0.32f;
    const float hy = pupilCy - pupilRy * 0.30f;
    const int r = s.catchlightR / 2 > 1 ? s.catchlightR / 2 : 1;
    face_.fillCircle(px(hx), py(hy), r, white_);
  }
}

void FaceRenderer::drawMouth(const FaceParams &params) {
  const FaceStyle &s = kStyles[styleIndex_];

  const float curve =
      (params.mouthCurve + s.mouthRestCurve) * s.mouthMaxCurve;

  int prevX = px(static_cast<float>(-s.mouthHalfWidth));
  int prevY = py(static_cast<float>(s.mouthHeight));
  for (int i = 1; i <= 12; ++i) {
    const float t = -1.0f + 2.0f * static_cast<float>(i) / 12.0f;
    const float x = t * s.mouthHalfWidth;
    const float y = s.mouthHeight + curve * (1.0f - t * t);
    const int nx = px(x);
    const int ny = py(y);
    // 線を 2 本重ねて太くする
    face_.drawLine(prevX, prevY, nx, ny, ink_);
    face_.drawLine(prevX, prevY + 1, nx, ny + 1, ink_);
    prevX = nx;
    prevY = ny;
  }
}

void FaceRenderer::drawPlushEye(int side, const FaceParams &params,
                                float open) {
  using namespace plush;

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

  // 白目
  face_.fillEllipse(px(cx), py(cy), kScleraRx, socketRy, white_);

  // 虹彩。視線に合わせて白目の中を動く。
  // 白目からはみ出さないよう、可動量を半径の差に収める。
  const float maxOx = static_cast<float>(kScleraRx - kIrisR);
  const float ox = clampf(params.eyeOffsetX * kIrisTravel, -maxOx, maxOx);

  const int irisRy = kIrisR < socketRy ? kIrisR : socketRy;
  const float maxOy = static_cast<float>(socketRy - irisRy);
  const float oy = clampf(params.eyeOffsetY * kIrisTravel, -maxOy, maxOy);

  const float ix = cx + ox;
  const float iy = cy + oy;

  // 虹彩の下に水色を覗かせる。水色で塗ってから、濃い円を少し上にずらして
  // 重ねると、下側だけが三日月として残る。
  face_.fillEllipse(px(ix), py(iy), kIrisR, irisRy, irisGlow_);
  const int innerRy = irisRy - 1 > 1 ? irisRy - 1 : 1;
  face_.fillEllipse(px(ix), py(iy + kGlowShift * open), kIrisR - 1, innerRy,
                    irisDark_);

  // ハイライト。生気のほとんどはここで決まる。
  if (irisRy >= 6) {
    face_.fillEllipse(px(ix - kIrisR * 0.38f), py(iy + irisRy * 0.36f), 4, 4,
                      white_);
    face_.fillEllipse(px(ix + kIrisR * 0.46f), py(iy - irisRy * 0.30f), 2, 2,
                      white_);
  }
}

void FaceRenderer::drawPlushFace(const FaceParams &params, float open) {
  using namespace plush;

  // 背景 (透過色) の上に、角丸の頭を 1 色で置く。顔パッチは持たない。
  face_.fillSprite(skin_);
  face_.fillRoundRect(px(kHeadLeft), py(kHeadTop), kHeadRight - kHeadLeft,
                      kHeadTop - kHeadBottom, kHeadCorner, fur_);

  drawPlushEye(-1, params, open);
  drawPlushEye(1, params, open);

  // 鼻。目と目の間に置く小さな丸。口は持たない。
  face_.fillEllipse(px(0.0f), py(kNoseY), kNoseRx, kNoseRy, nose_);

  drawPlushBrow(-1, params);
  drawPlushBrow(1, params);
}

// 眉。browAngle が 0 のときは描かない。
// 素の顔に眉が無いぶん、出たときの印象が強くなる。
void FaceRenderer::drawPlushBrow(int side, const FaceParams &params) {
  using namespace plush;

  if (params.browAngle > -0.05f && params.browAngle < 0.05f) {
    return;
  }

  const float a = clampf(params.browAngle, -1.0f, 1.0f);

  // 内側 = 顔の中心寄り。怒ると内側が下がり、困ると内側が上がる。
  const float innerX = static_cast<float>(side * (kEyeSpacing - kBrowHalfWidth));
  const float outerX = static_cast<float>(side * (kEyeSpacing + kBrowHalfWidth));
  const float innerY = kBrowY - a * kBrowInnerDrop;
  const float outerY = kBrowY + a * kBrowOuterLift;

  for (int d = 0; d < 3; ++d) {
    face_.drawLine(px(innerX), py(innerY) + d, px(outerX), py(outerY) + d, irisDark_);
  }
}

void FaceRenderer::drawSimpleFace(const FaceParams &params, float open) {
  const FaceStyle &s = kStyles[styleIndex_];

  face_.fillSprite(skin_);

  if (s.blush) {
    const int by = s.eyeHeight - 22;
    face_.fillEllipse(px(-46.0f), py(static_cast<float>(by)), 13, 8, blush_);
    face_.fillEllipse(px(46.0f), py(static_cast<float>(by)), 13, 8, blush_);
  }

  drawEye(-1, params, open);
  drawEye(1, params, open);

  drawMouth(params);
}

void FaceRenderer::drawFaceUpright(const FaceParams &params) {
  const float open = clampf(params.eyeOpen, 0.0f, 1.0f);

  if (kStyles[styleIndex_].kind == FaceKind::Plush) {
    drawPlushFace(params, open);
  } else {
    drawSimpleFace(params, open);
  }
}

void FaceRenderer::draw(const FaceParams &params, const char *label) {
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

  if (label != nullptr) {
    // 顔を回した後に重ねるので、ラベル自体は傾かない
    frame_.setTextColor(ink_);
    frame_.setTextDatum(bottom_center);
    frame_.setTextSize(1);
    frame_.drawString(label, kCenter, kSize - 3);
  }

  frame_.pushSprite(0, 0);
}

}  // namespace pet
