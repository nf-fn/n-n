// フェーズ 0: ハードウェア確認スケッチ
//
// 仕様書の「実機で確認が必要な未確定事項」を潰すための使い捨てコード。
// フェーズ 1 でポケットペット本体に置き換える。
//
// 確認済み (2026-09-22):
//   - M5Unified 0.2.7 が BMI270 を認識する
//   - PSRAM 8MB 有効 / Flash 8MB / 画面 128x128
//
// 残り: IMU の軸の向きと画面の向きの対応
//
// 使い方:
//   画面に出る姿勢を取り、静止させた状態で画面(ボタン)を押す。
//   押すたびに 50 サンプルの平均が記録される。4 姿勢すべてで押すと
//   対応表が出力される。

#include <M5Unified.h>

struct Pose {
  const char *label;       // 画面表示 (ASCII のみ)
  const char *description; // シリアル出力用
};

static const Pose kPoses[] = {
    {"FLAT", "画面を上に向けて水平"},
    {"FACE ME", "画面を手前(自分)に向けて垂直"},
    {"TILT R", "水平から右に傾ける"},
    {"UPSIDE", "画面を下に向けて逆さ"},
};
static const int kPoseCount = sizeof(kPoses) / sizeof(kPoses[0]);

struct Reading {
  float ax, ay, az;
};

static Reading captured[kPoseCount];
static int poseIndex = 0;

static void showPrompt() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);

  if (poseIndex >= kPoseCount) {
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.setTextSize(2);
    M5.Display.drawString("DONE", 64, 64);
    return;
  }

  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextSize(1);
  M5.Display.drawString("pose", 64, 34);

  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setTextSize(2);
  M5.Display.drawString(kPoses[poseIndex].label, 64, 62);

  M5.Display.setTextColor(TFT_DARKGREY);
  M5.Display.setTextSize(1);
  M5.Display.drawString("then press", 64, 92);

  char progress[16];
  snprintf(progress, sizeof(progress), "%d / %d", poseIndex + 1, kPoseCount);
  M5.Display.drawString(progress, 64, 108);
}

// 静止している前提で 50 サンプルを平均し、ノイズを均す
static Reading captureAveraged() {
  Reading sum{0.0f, 0.0f, 0.0f};
  int taken = 0;
  while (taken < 50) {
    if (M5.Imu.update()) {
      const auto d = M5.Imu.getImuData();
      sum.ax += d.accel.x;
      sum.ay += d.accel.y;
      sum.az += d.accel.z;
      ++taken;
    }
    delay(5);
  }
  return Reading{sum.ax / taken, sum.ay / taken, sum.az / taken};
}

static void printSummary() {
  Serial.println();
  Serial.println("=== 軸の対応表 ===");
  Serial.println("姿勢                          ax      ay      az");
  for (int i = 0; i < kPoseCount; ++i) {
    Serial.printf("%-28s %+.3f  %+.3f  %+.3f\n", kPoses[i].description,
                  captured[i].ax, captured[i].ay, captured[i].az);
  }
  Serial.println();
  Serial.println("重力は -1g 側に出る軸が『上』を向いている軸。");
  Serial.println("=== 記録完了 ===");
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = true;
  M5.begin(cfg);

  Serial.begin(115200);
  delay(1500);  // USB CDC が上がるのを待つ

  Serial.println();
  Serial.println("=== 軸の向き確認 ===");
  Serial.printf("IMU: %s / PSRAM: %u bytes / 画面: %dx%d\n",
                M5.Imu.getType() == m5::imu_t::imu_bmi270 ? "BMI270" : "不明",
                (unsigned)ESP.getPsramSize(), M5.Display.width(),
                M5.Display.height());
  Serial.println("画面の指示どおりの姿勢で静止させ、画面を押すこと。");
  Serial.println();

  showPrompt();
}

void loop() {
  M5.update();

  if (poseIndex < kPoseCount && M5.BtnA.wasPressed()) {
    Serial.printf("[%d/%d] %s ... 計測中\n", poseIndex + 1, kPoseCount,
                  kPoses[poseIndex].description);

    // 押した指の揺れが収まるのを待つ
    delay(400);
    captured[poseIndex] = captureAveraged();

    Serial.printf("      ax=%+.3f ay=%+.3f az=%+.3f\n", captured[poseIndex].ax,
                  captured[poseIndex].ay, captured[poseIndex].az);

    ++poseIndex;
    showPrompt();

    if (poseIndex == kPoseCount) {
      printSummary();
    }
  }

  delay(10);
}
