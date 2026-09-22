// 実機で録った CSV を native テストに流し込むための読み込み。
//
// 録り方:
//   pio run -e record -t upload --upload-port <port>
//   ~/.platformio/penv/bin/python tools/capture_fixtures.py <port>
#pragma once

#include <string>
#include <vector>

#include "Types.h"

namespace pet_test {

// test/fixtures/<name>.csv を読む。見つからなければ空を返す。
std::vector<pet::ImuSample> loadFixture(const std::string &name);

}  // namespace pet_test
