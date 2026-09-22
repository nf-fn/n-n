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

    // 5: 参考画像から起こした案。白い毛の頭に耳、グレージュの顔パッチ、
    //    青い虹彩に大きなハイライト、小さな鼻。口は持たない。
    //    造作の構成が他と違うため、寸法は drawPlushFace 側の定数で持つ。
    {"PLUSH", FaceKind::Plush, 245, 205, 120,
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

constexpr int kHeadCx = 0;
constexpr int kHeadCy = -10;
constexpr int kHeadRx = 58;
constexpr int kHeadRy = 56;

constexpr int kEarOffsetX = 40;
constexpr int kEarY = 42;
constexpr int kEarR = 19;

constexpr int kPatchCx = 0;
constexpr int kPatchCy = -14;
constexpr int kPatchRx = 37;
constexpr int kPatchRy = 32;

constexpr int kEyeSpacing = 20;
constexpr int kEyeY = -8;
constexpr int kSocketR = 14;  // 目の外形 (濃い輪郭)
constexpr int kIrisR = 12;    // 青い虹彩
constexpr int kPupilR = 5;    // 黒い瞳孔

// 視線の可動量。虹彩ごと動かしたうえで、瞳孔をさらに動かす。
constexpr float kIrisTravel = 2.5f;
constexpr float kPupilTravel = 3.0f;

constexpr int kNoseR = 4;
constexpr int kNoseY = -13;

// 眉。素の顔には無く、怒り・困りのときだけ現れる。
constexpr int kBrowY = 14;        // 目の上
constexpr int kBrowHalfWidth = 12;
constexpr float kBrowInnerDrop = 6.0f;  // 怒ったとき内側が下がる量
constexpr float kBrowOuterLift = 2.5f;  // 同じく外側が上がる量

}  // namespace plush

}  // namespace

bool FaceRenderer::begin() {
  M5.Display.setRotation(0);

  ink_ = M5.Display.color565(42, 33, 24);
  white_ = M5.Display.color565(255, 252, 245);
  blush_ = M5.Display.color565(244, 160, 150);

  fur_ = M5.Display.color565(250, 250, 248);
  patch_ = M5.Display.color565(176, 166, 158);
  iris_ = M5.Display.color565(74, 150, 232);

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

  const int socketRy = static_cast<int>(std::lround(kSocketR * open));
  if (socketRy < 3) {
    // 閉じた目の弧。向きで意味が変わるので eyeArch から決める。
    //   eyeArch  0 → 中央がわずかに下がる ∪ (まばたき)
    //   eyeArch +1 → 中央が持ち上がる ∩ (笑い)
    //   eyeArch -1 → 深い ∪ (眠い)
    const float arch = -3.0f + params.eyeArch * 7.0f;

    int prevX = px(cx - kSocketR);
    int prevY = py(cy);
    for (int i = 1; i <= 10; ++i) {
      const float t = -1.0f + 2.0f * static_cast<float>(i) / 10.0f;
      const float x = cx + t * kSocketR;
      const float y = cy + arch * (1.0f - t * t);
      const int nx = px(x);
      const int ny = py(y);
      // 3 本重ねて太くする。細いと笑っているように見えない。
      face_.drawLine(prevX, prevY, nx, ny, ink_);
      face_.drawLine(prevX, prevY + 1, nx, ny + 1, ink_);
      face_.drawLine(prevX, prevY + 2, nx, ny + 2, ink_);
      prevX = nx;
      prevY = ny;
    }
    return;
  }

  // 目の外形。虹彩を縁取って輪郭を作る。
  face_.fillEllipse(px(cx), py(cy), kSocketR, socketRy, ink_);

  // 虹彩と瞳孔は視線に合わせて動く。虹彩が先に動き、瞳孔がさらに動くと、
  // 眼球が回っているように見える。
  const float ix = cx + params.eyeOffsetX * kIrisTravel;
  const float iy = cy + params.eyeOffsetY * kIrisTravel;
  const int irisRy = static_cast<int>(std::lround(kIrisR * open));
  if (irisRy < 2) {
    return;
  }
  face_.fillEllipse(px(ix), py(iy), kIrisR, irisRy, iris_);

  const float pxx = ix + params.eyeOffsetX * kPupilTravel;
  const float pyy = iy + params.eyeOffsetY * kPupilTravel;
  const int pupilRy = kPupilR < irisRy ? kPupilR : irisRy;
  face_.fillEllipse(px(pxx), py(pyy), kPupilR, pupilRy, ink_);

  // ハイライト。生気のほとんどはここで決まる。
  if (irisRy >= 6) {
    face_.fillCircle(px(ix - kIrisR * 0.40f), py(iy + kIrisR * 0.40f), 4,
                     white_);
    face_.fillCircle(px(ix + kIrisR * 0.42f), py(iy - kIrisR * 0.34f), 2,
                     white_);
  }
}

void FaceRenderer::drawPlushFace(const FaceParams &params, float open) {
  using namespace plush;

  // 背景 (透過色) の上に、白い毛の頭と耳を置く
  face_.fillSprite(skin_);
  face_.fillCircle(px(-kEarOffsetX), py(kEarY), kEarR, fur_);
  face_.fillCircle(px(kEarOffsetX), py(kEarY), kEarR, fur_);
  face_.fillEllipse(px(kHeadCx), py(kHeadCy), kHeadRx, kHeadRy, fur_);

  // 顔のグレージュ部分
  face_.fillEllipse(px(kPatchCx), py(kPatchCy), kPatchRx, kPatchRy, patch_);

  drawPlushEye(-1, params, open);
  drawPlushEye(1, params, open);

  // 鼻。口は持たない。
  face_.fillEllipse(px(0.0f), py(kNoseY), kNoseR, kNoseR - 1, ink_);

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
    face_.drawLine(px(innerX), py(innerY) + d, px(outerX), py(outerY) + d, ink_);
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
