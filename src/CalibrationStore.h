// [I/O] キャリブレーションの結果を NVS に保存する層。
//
// 保存形式にはバージョンを付ける。構造体を変えたときに古いデータを
// そのまま読むと壊れた閾値が入るため、合わなければ既定値へ戻す。
#pragma once

#include "Calibration.h"

namespace pet {

class CalibrationStore {
 public:
  // 構造体を変えたらこの値を上げる。古い保存は読み捨てられる。
  static constexpr uint8_t kFormatVersion = 1;

  // 保存されていれば読む。無ければ空のまま true を返す。
  bool load(CalibrationSet &out);

  bool save(const CalibrationSet &set);

  // すべて消して既定値に戻す。
  bool clear();
};

}  // namespace pet
