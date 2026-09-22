#include "ImuSource.h"

#include <M5Unified.h>

namespace pet {

void ImuSource::begin() {
  // M5.begin() 側で初期化済み。軸補正が不要なためここで行うことはない。
}

bool ImuSource::read(ImuSample &out) {
  if (!M5.Imu.update()) {
    return false;
  }

  const auto d = M5.Imu.getImuData();
  out.ax = d.accel.x;
  out.ay = d.accel.y;
  out.az = d.accel.z;
  out.gx = d.gyro.x;
  out.gy = d.gyro.y;
  out.gz = d.gyro.z;
  out.tMs = millis();
  return true;
}

}  // namespace pet
