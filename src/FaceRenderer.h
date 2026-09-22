// [I/O] FaceParams を画面に描く層。
//
// 顔は 128x128 のスプライトに水平な状態で描き、それを回転させて
// フレームバッファへ合成する。こうすると目や口の形そのものが傾くため、
// 座標だけ回す実装より傾きが自然に見える。
#pragma once

#include <M5Unified.h>

#include "Types.h"

namespace pet {

class FaceRenderer {
 public:
  bool begin();
  void draw(const FaceParams &params);

 private:
  void drawFaceUpright(const FaceParams &params);

  M5Canvas frame_{&M5.Display};  // 画面へ一括転送するバッファ
  M5Canvas face_{&M5.Display};   // 傾ける前の顔

  uint16_t skin_ = 0;
  uint16_t ink_ = 0;
  uint16_t white_ = 0;

  bool ready_ = false;
};

}  // namespace pet
