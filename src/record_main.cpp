// モーション録音ファームウェア (env:record)
//
// IMU の生データを CSV でシリアルに吐き、test/fixtures/*.csv を作る。
// ペット本体 (src/main.cpp) とは排他でビルドされる。
//
// 使い方:
//   ~/.platformio/penv/bin/pio run -e record -t upload --upload-port <port>
//   python3 tools/capture_fixtures.py <port>
//
// 画面に出る動作名のとおりに動かす。画面を押すと準備時間のカウントダウンが
// 始まり、0 になったら録音が始まる。ポケットに入れる動作は準備時間を長く
// 取ってある。

#include <M5Unified.h>

#include "ImuSource.h"

namespace {

struct Motion {
  const char *name;       // 画面表示とファイル名 (ASCII)
  const char *guide;      // シリアルに出す指示
  uint32_t prepMs;        // 押してから録音開始までの準備時間
  uint32_t durationMs;    // 録音する長さ
};

// 各動作を 1 本ずつ録る。walk_pocket は誤検出を防ぐための負例なので必須。
const Motion kMotions[] = {
    {"IDLE", "机に置いて、いっさい触らない", 3000, 10000},
    {"TAP", "指先で数回つつく。1 回ずつ間をあける", 3000, 8000},
    {"SHAKE", "手に持って振る", 3000, 6000},
    {"STROKE", "指で本体の面や側面をゆっくり撫で続ける", 3000, 8000},
    {"LIFT", "机から持ち上げて、また置く。数回繰り返す", 3000, 10000},
    {"SPIN", "手のひらの上でくるくる回す", 3000, 6000},
    {"WALK", "ポケットに入れて普通に歩く", 10000, 20000},
};
constexpr int kMotionCount = sizeof(kMotions) / sizeof(kMotions[0]);

enum class State { Waiting, Preparing, Recording, Done };

pet::ImuSource imu;

State state = State::Waiting;
int motionIndex = 0;
uint32_t phaseStartMs = 0;
uint32_t lastSampleMs = 0;

// 秒表示の再描画を 1 秒に 1 回に抑えるための記憶。
// 状態が変わるたびに -1 に戻す。
int lastShownSecond = -1;

constexpr uint32_t kSampleIntervalMs = 10;  // 100Hz

void showBig(const char *text, uint16_t color) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(color);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString(text, 64, 58);
}

void showWaiting() {
  showBig(kMotions[motionIndex].name, TFT_YELLOW);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY);
  M5.Display.drawString("press", 64, 88);

  char progress[16];
  snprintf(progress, sizeof(progress), "%d / %d", motionIndex + 1,
           kMotionCount);
  M5.Display.drawString(progress, 64, 104);
}

void showCountdown(int secondsLeft) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", secondsLeft);
  showBig(buf, TFT_ORANGE);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY);
  M5.Display.drawString(kMotions[motionIndex].name, 64, 92);
}

void showRecording(int secondsLeft) {
  char buf[16];
  snprintf(buf, sizeof(buf), "REC %d", secondsLeft);
  showBig(buf, TFT_RED);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY);
  M5.Display.drawString(kMotions[motionIndex].name, 64, 92);
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = true;
  M5.begin(cfg);

  Serial.begin(115200);
  imu.begin();

  delay(1500);  // USB CDC が上がるのを待つ

  Serial.println();
  Serial.println("#READY モーション録音");
  Serial.printf("#COUNT %d\n", kMotionCount);
  for (int i = 0; i < kMotionCount; ++i) {
    Serial.printf("#MOTION %s %s\n", kMotions[i].name, kMotions[i].guide);
  }

  showWaiting();
}

void loop() {
  M5.update();
  const uint32_t now = millis();

  switch (state) {
    case State::Waiting: {
      if (M5.BtnA.wasPressed()) {
        state = State::Preparing;
        phaseStartMs = now;
        lastShownSecond = -1;
        Serial.printf("#PREP %s %s\n", kMotions[motionIndex].name,
                      kMotions[motionIndex].guide);
      }
      break;
    }

    case State::Preparing: {
      const uint32_t elapsed = now - phaseStartMs;
      const uint32_t total = kMotions[motionIndex].prepMs;
      if (elapsed >= total) {
        state = State::Recording;
        phaseStartMs = now;
        lastSampleMs = now;
        lastShownSecond = -1;
        Serial.printf("#BEGIN %s %u\n", kMotions[motionIndex].name,
                      (unsigned)kMotions[motionIndex].durationMs);
        Serial.println("t,ax,ay,az,gx,gy,gz");
        showRecording(
            static_cast<int>(kMotions[motionIndex].durationMs / 1000));
      } else {
        const int left = static_cast<int>((total - elapsed) / 1000) + 1;
        if (left != lastShownSecond) {
          lastShownSecond = left;
          showCountdown(left);
        }
      }
      break;
    }

    case State::Recording: {
      const uint32_t elapsed = now - phaseStartMs;

      if (now - lastSampleMs >= kSampleIntervalMs) {
        lastSampleMs = now;
        pet::ImuSample s;
        if (imu.read(s)) {
          Serial.printf("%u,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f\n",
                        (unsigned)elapsed, s.ax, s.ay, s.az, s.gx, s.gy, s.gz);
        }
      }

      if (elapsed >= kMotions[motionIndex].durationMs) {
        Serial.printf("#END %s\n", kMotions[motionIndex].name);
        ++motionIndex;
        if (motionIndex >= kMotionCount) {
          state = State::Done;
          Serial.println("#DONE");
          showBig("DONE", TFT_GREEN);
        } else {
          state = State::Waiting;
          lastShownSecond = -1;
          showWaiting();
        }
      } else {
        const int left =
            static_cast<int>((kMotions[motionIndex].durationMs - elapsed) /
                             1000) + 1;
        if (left != lastShownSecond) {
          lastShownSecond = left;
          showRecording(left);
        }
      }
      break;
    }

    case State::Done:
      break;
  }

  delay(1);
}
