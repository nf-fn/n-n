// [I/O] FaceParams を画面に描く層。
//
// 顔は水平な状態でスプライトに描き、それを回転させてフレームへ合成する。
// こうすると目や口の形そのものが傾くため、座標だけ回す実装より自然に見える。
//
// 造作は「淡いグレーの角丸の頭 / 白目の中の濃紺の虹彩 / その下に覗く水色 /
// 大きな白ハイライト / 小さな横長の鼻」。口も耳も持たない。
// 感情は目と眉だけで表す。
#pragma once

#include <M5Unified.h>

#include "Types.h"

namespace pet {

class FaceRenderer {
 public:
  bool begin();
  void draw(const FaceParams &params);

 private:
  void drawFace(const FaceParams &params, float open);
  void drawEye(int side, const FaceParams &params, float open);
  void drawBrow(int side, const FaceParams &params);

  M5Canvas frame_{&M5.Display};  // 画面へ一括転送するバッファ
  M5Canvas face_{&M5.Display};   // 傾ける前の顔

  uint16_t bg_ = 0;         // 頭の外側
  uint16_t head_ = 0;       // 頭のグレー
  uint16_t white_ = 0;      // 白目とハイライト
  uint16_t irisDark_ = 0;   // 虹彩の濃紺。閉じた目の弧や眉にも使う
  uint16_t irisGlow_ = 0;   // 虹彩の下に覗く水色
  uint16_t nose_ = 0;

  bool ready_ = false;
};

}  // namespace pet
