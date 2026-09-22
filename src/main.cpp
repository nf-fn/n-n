// ポケットペット for M5Stack AtomS3R
//
// 仕様: docs/superpowers/specs/2026-09-22-pocket-pet-design.md
//
// フェーズ 1: 傾けると目玉が転がる。まばたきする。
//   イベント検出 (撫でる/振る/叩く) と気分はフェーズ 2 以降。

#include <M5Unified.h>

#include "FaceComposer.h"
#include "FaceRenderer.h"
#include "ImuSource.h"
#include "MotionAnalyzer.h"

namespace {

constexpr uint32_t kSensorIntervalMs = 10;  // 100Hz
constexpr uint32_t kRenderIntervalMs = 33;  // 約 30fps

pet::ImuSource imu;
pet::MotionAnalyzer analyzer;
pet::FaceComposer composer;
pet::FaceRenderer renderer;

uint32_t lastSensorMs = 0;
uint32_t lastRenderMs = 0;

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = true;
  M5.begin(cfg);

  Serial.begin(115200);

  imu.begin();

  if (!renderer.begin()) {
    // スプライトを確保できない場合は描画せず、その旨だけ出す
    M5.Display.fillScreen(TFT_RED);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("NO MEM", 64, 64);
    Serial.println("スプライトを確保できませんでした");
    return;
  }

  Serial.println("ポケットペット起動 (フェーズ1)");
}

void loop() {
  M5.update();

  const uint32_t now = millis();

  if (now - lastSensorMs >= kSensorIntervalMs) {
    lastSensorMs = now;
    pet::ImuSample sample;
    if (imu.read(sample)) {
      analyzer.update(sample);
    }
  }

  if (now - lastRenderMs >= kRenderIntervalMs) {
    lastRenderMs = now;
    renderer.draw(composer.compose(analyzer.posture(), now));
  }

  delay(1);
}
