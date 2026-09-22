// 触られ方から気分を作る層。
//
// 永続状態は持たない。電源が入っているあいだだけ気分が連続する。
// 振られ続ければ怒りが溜まり、撫でれば落ち着く。この短期的な慣性が、
// 保存のないペットに手応えを生む中心的な仕掛けになる。
#pragma once

#include "Types.h"

namespace pet {

class Mood {
 public:
  // 姿勢も渡す。画面を伏せられたら寝かしつけられたものとして扱うため。
  void update(MotionEvent event, Activity activity, const Posture &posture,
              float dt);

  const MoodState &state() const { return state_; }

 private:
  MoodState state_;
};

}  // namespace pet
