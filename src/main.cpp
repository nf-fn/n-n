// ポケットペット for M5Stack AtomS3R
//
// 仕様: docs/superpowers/specs/2026-09-22-pocket-pet-design.md
//
// フェーズ 1: 傾けると目玉が転がる。まばたきする。
//   イベント検出 (撫でる/振る/叩く) と気分はフェーズ 2 以降。
//
// 顔は PLUSH に決定した。口を持たないため、感情は目と眉だけで表す。
// 画面を押すと表情のプレビューを切り替える。表情を気分に繋ぐのはフェーズ 3。

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

// 表情のプレビュー。気分に繋ぐのはフェーズ 3 なので、それまでは手で切り替える。
// 目の位置 (視線) は姿勢のまま残し、表情に関わる値だけ上書きする。
struct Expression {
  const char *name;
  float eyeOpen;
  float eyeArch;
  float browAngle;
  bool override;  // false なら素のまま (まばたきも生きる)
};

constexpr Expression kExpressions[] = {
    {"NORMAL", 1.0f, 0.0f, 0.0f, false},
    {"HAPPY", 0.02f, 1.0f, 0.0f, true},
    {"ANGRY", 0.80f, 0.0f, 1.0f, true},
    {"WORRIED", 0.75f, 0.0f, -1.0f, true},
    {"SLEEPY", 0.04f, -1.0f, 0.0f, true},
};
constexpr int kExpressionCount =
    sizeof(kExpressions) / sizeof(kExpressions[0]);

int expressionIndex = 0;

// 切り替えた直後だけ名前を重ねて出す。
constexpr uint32_t kLabelHoldMs = 1500;
uint32_t labelUntilMs = 0;

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
  Serial.printf("顔: %s / 画面を押すと表情が切り替わります (全 %d 種)\n",
                renderer.styleName(), kExpressionCount);

  labelUntilMs = millis() + kLabelHoldMs;
}

void loop() {
  M5.update();

  const uint32_t now = millis();

  if (M5.BtnA.wasPressed()) {
    expressionIndex = (expressionIndex + 1) % kExpressionCount;
    labelUntilMs = now + kLabelHoldMs;
    Serial.printf("表情: %s\n", kExpressions[expressionIndex].name);
  }

  if (now - lastSensorMs >= kSensorIntervalMs) {
    lastSensorMs = now;
    pet::ImuSample sample;
    if (imu.read(sample)) {
      analyzer.update(sample);
    }
  }

  if (now - lastRenderMs >= kRenderIntervalMs) {
    lastRenderMs = now;

    pet::FaceParams params = composer.compose(analyzer.posture(), now);

    const Expression &e = kExpressions[expressionIndex];
    if (e.override) {
      // 視線 (eyeOffset) と顔の傾きは姿勢のまま。表情だけ差し替える。
      params.eyeOpen = e.eyeOpen;
      params.eyeArch = e.eyeArch;
      params.browAngle = e.browAngle;
    }

    const char *label = (now < labelUntilMs) ? e.name : nullptr;
    renderer.draw(params, label);
  }

  delay(1);
}
