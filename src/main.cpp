// ポケットペット for M5Stack AtomS3R
//
// 仕様: docs/superpowers/specs/2026-09-22-pocket-pet-design.md
//
// 触られ方 → 気分 → 顔と声。
//
//   ImuSource      [I/O]  IMU を読む
//   MotionAnalyzer [純粋] 姿勢・つつき・持ち上げ・撫で/手持ち/振りを判定
//   Mood           [純粋] 出来事と状態から気分を作る
//   FaceComposer   [純粋] 気分を顔のパラメータに落とす
//   FaceRenderer   [I/O]  顔を描く
//   VoiceComposer  [純粋] 気分を音符の並びに落とす
//   VoiceOutput    [I/O]  波形を合成して鳴らす
//
// 純粋層は M5 に一切依存せず、Mac 上でテストできる (pio test -e native)。

#include <M5Unified.h>

#include "Calibration.h"
#include "CalibrationStore.h"
#include "CalibrationUi.h"
#include "FaceComposer.h"
#include "FaceRenderer.h"
#include "ImuSource.h"
#include "Mood.h"
#include "MotionAnalyzer.h"
#include "Voice.h"
#include "VoiceOutput.h"

namespace {

constexpr uint32_t kSensorIntervalMs = 10;  // 100Hz
constexpr uint32_t kRenderIntervalMs = 33;  // 約 30fps

// 実機で聞きながら決めた値。0-255。
constexpr uint8_t kVolume = 20;

pet::ImuSource imu;
pet::MotionAnalyzer analyzer;
pet::Mood mood;
pet::FaceComposer composer;
pet::FaceRenderer renderer;
pet::VoiceComposer voice;
pet::VoiceOutput speaker;

pet::CalibrationStore calibStore;
pet::CalibrationSet calibration;
pet::CalibrationUi calibUi;

// 長押しと短押しを分ける閾値 [ms]。つつくつもりで設定に入ると困るので
// 既定より長めに取る。
constexpr uint32_t kHoldThreshMs = 700;

uint32_t lastSensorMs = 0;
uint32_t lastRenderMs = 0;

// ボタンはセンサーの周期とずれて押されるので、消費するまで保持する。
// その場で見るだけだと、タイミング次第で押下が消える。
bool buttonPending = false;

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = true;
  // Atomic Echo Base のスピーカー。M5Unified が ES8311 の初期化まで行う。
  cfg.external_speaker.atomic_echo = true;
  M5.begin(cfg);

  Serial.begin(115200);

  imu.begin();

  M5.BtnA.setHoldThresh(kHoldThreshMs);

  // 保存してあるキャリブレーションを読み、閾値に反映する。
  // 何も無ければ既定値のまま。
  calibStore.load(calibration);
  analyzer.setThresholds(pet::resolve(calibration));
  calibUi.begin(&calibStore, &calibration);

  if (!speaker.begin(kVolume)) {
    Serial.println("スピーカーが見つかりません (Echo Base の接続を確認)");
  }

  if (!renderer.begin()) {
    M5.Display.fillScreen(TFT_RED);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("NO MEM", 64, 64);
    Serial.println("スプライトを確保できませんでした");
    return;
  }

  Serial.println("ポケットペット起動");
}

void loop() {
  M5.update();

  const uint32_t now = millis();

  // --- キャリブレーション中は画面とボタンを明け渡す ---
  if (calibUi.active()) {
    pet::ImuSample sample;
    const bool haveSample = imu.read(sample);
    if (calibUi.update(now, haveSample ? &sample : nullptr)) {
      // 設定が変わったので閾値を入れ直す
      analyzer.setThresholds(pet::resolve(calibration));
    }
    if (!calibUi.active()) {
      // 抜けた直後は顔を描き直す
      lastRenderMs = 0;
    }
    delay(1);
    return;
  }

  // 長押しでキャリブレーションへ。短押しは「つつく」のまま。
  if (M5.BtnA.wasHold()) {
    calibUi.enter();
    return;
  }
  if (M5.BtnA.wasClicked()) {
    buttonPending = true;
  }

  if (now - lastSensorMs >= kSensorIntervalMs) {
    lastSensorMs = now;

    pet::ImuSample sample;
    if (imu.read(sample)) {
      analyzer.update(sample);

      pet::MotionEvent event = analyzer.event();
      if (event == pet::MotionEvent::None && buttonPending) {
        event = pet::MotionEvent::Tap;
        buttonPending = false;
      }

      mood.update(event, analyzer.activity(), analyzer.posture(),
                  kSensorIntervalMs / 1000.0f);

      const pet::VoiceCue cue = voice.update(event, mood.state(), now);
      if (!cue.empty()) {
        speaker.play(cue);
      }
    }
  }

  if (now - lastRenderMs >= kRenderIntervalMs) {
    lastRenderMs = now;
    renderer.draw(composer.compose(analyzer.posture(), mood.state(), now));
  }

  delay(1);
}
