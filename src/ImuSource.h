// [I/O] M5Unified の IMU を ImuSample に変換する層。
//
// フェーズ 0 の実測により、BMI270 の軸は画面座標と一致していることが
// 分かっている (+X 右 / +Y 上 / +Z 手前)。よって軸の入れ替えも符号反転も
// 行わない。将来ハードが変わった場合の補正はこのクラスに閉じる。
#pragma once

#include "Types.h"

namespace pet {

class ImuSource {
 public:
  void begin();

  // 新しいサンプルが取れたときだけ true を返す。
  bool read(ImuSample &out);
};

}  // namespace pet
