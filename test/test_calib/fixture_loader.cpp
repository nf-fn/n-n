#include "fixture_loader.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace pet_test {
namespace {

// pio test はプロジェクトルートから走るため、この相対パスで届く。
const char *kFixtureDir = "test/fixtures";

}  // namespace

std::vector<pet::ImuSample> loadFixture(const std::string &name) {
  std::vector<pet::ImuSample> out;

  const std::string path = std::string(kFixtureDir) + "/" + name + ".csv";
  std::ifstream in(path);
  if (!in) {
    std::fprintf(stderr, "フィクスチャを開けません: %s\n", path.c_str());
    return out;
  }

  std::string line;
  std::getline(in, line);  // ヘッダ行を読み飛ばす

  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    std::istringstream ss(line);
    std::string cell;
    float v[7] = {0};
    int i = 0;
    while (i < 7 && std::getline(ss, cell, ',')) {
      v[i++] = std::strtof(cell.c_str(), nullptr);
    }
    if (i < 7) {
      continue;
    }

    pet::ImuSample s;
    s.tMs = static_cast<uint32_t>(v[0]);
    s.ax = v[1];
    s.ay = v[2];
    s.az = v[3];
    s.gx = v[4];
    s.gy = v[5];
    s.gz = v[6];
    out.push_back(s);
  }

  return out;
}

}  // namespace pet_test
