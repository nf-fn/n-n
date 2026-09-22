// [I/O] FaceParams を画面に描く層。
//
// 顔は 128x128 のスプライトに水平な状態で描き、それを回転させて
// フレームバッファへ合成する。こうすると目や口の形そのものが傾くため、
// 座標だけ回す実装より傾きが自然に見える。
//
// 顔の見た目は Style として表で持つ。実機で見比べて 1 つに決めるため、
// 実行中に切り替えられるようにしてある。
#pragma once

#include <M5Unified.h>

#include "Types.h"

namespace pet {

// 描き方の系統。造作の構成そのものが違うので、表の値では吸収できない。
enum class FaceKind {
  Simple,  // 肌色の地に目と口だけを置く
  Plush,   // 白い毛の頭・耳・顔パッチ・青い目・鼻。口は無い
};

// 顔の造作。すべて顔座標 (原点 = 中心、+X 右 / +Y 上、単位 = ピクセル)。
// Plush では eye/mouth 系の値は使わず、描画側の定数で組む。
struct FaceStyle {
  const char *name;
  FaceKind kind;

  uint8_t skinR, skinG, skinB;

  int eyeSpacing;  // 中心から左右の目までの距離
  int eyeHeight;   // 目の高さ。負の値は中心より下
  int eyeRx;       // 目の横半径
  int eyeRy;       // 目の縦半径

  bool hasSclera;  // true = 白目の中で黒目が動く / false = 目全体が動く
  int pupilR;      // 黒目の半径 (hasSclera のときのみ)
  int travel;      // 視線の可動量 [px]

  int catchlights;   // 瞳の光の数 (0-2)
  int catchlightR;

  int mouthHeight;
  int mouthHalfWidth;
  int mouthMaxCurve;
  float mouthRestCurve;  // 無表情時の口の曲がり。正で笑い

  bool blush;
};

constexpr int kStyleCount = 6;
extern const FaceStyle kStyles[kStyleCount];

// 起動時に出す案。見比べた結果ここに落ち着いた。
constexpr int kDefaultStyle = 5;

class FaceRenderer {
 public:
  bool begin();

  // label に文字列を渡すと画面下端に重ねて表示する。
  // デザインを見比べる間だけ使う仮のもの。
  void draw(const FaceParams &params, const char *label = nullptr);

  void setStyle(int index);
  int style() const { return styleIndex_; }
  const char *styleName() const { return kStyles[styleIndex_].name; }

 private:
  void drawFaceUpright(const FaceParams &params);
  void drawSimpleFace(const FaceParams &params, float open);
  void drawPlushFace(const FaceParams &params, float open);
  void drawEye(int side, const FaceParams &params, float open);
  void drawPlushEye(int side, const FaceParams &params, float open);
  void drawPlushBrow(int side, const FaceParams &params);
  void drawMouth(const FaceParams &params);

  M5Canvas frame_{&M5.Display};  // 画面へ一括転送するバッファ
  M5Canvas face_{&M5.Display};   // 傾ける前の顔

  int styleIndex_ = 0;

  uint16_t skin_ = 0;
  uint16_t ink_ = 0;
  uint16_t white_ = 0;
  uint16_t blush_ = 0;

  // Plush 用
  uint16_t fur_ = 0;        // 頭のグレー
  uint16_t irisDark_ = 0;   // 虹彩の濃紺。閉じた目の弧や眉にも使う
  uint16_t irisGlow_ = 0;   // 虹彩の下に覗く水色
  uint16_t nose_ = 0;

  bool ready_ = false;
};

}  // namespace pet
