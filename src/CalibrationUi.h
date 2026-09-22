// [I/O] キャリブレーションの画面。
//
// ボタンが 1 つしかないので、短押し = 次へ / 長押し = 決定。
// 運用中に長押しで入り、EXIT で戻る。
//
// 流れ: メニュー → 動作を選ぶ → 測定 → 判定 → テスト → 保存/破棄
//
// テストの段階が肝。導出した閾値で実際に検出を走らせ、検出回数を画面に
// 出す。効いているか確かめてから保存でき、気に入らなければ破棄できる。
#pragma once

#include <M5Unified.h>

#include "Calibration.h"
#include "CalibrationStore.h"

namespace pet {

class CalibrationUi {
 public:
  void begin(CalibrationStore *store, CalibrationSet *set);

  bool active() const { return screen_ != Screen::Off; }

  // 運用中に長押しされたら呼ぶ。
  void enter();

  // 毎ループ呼ぶ。active なら画面とボタンを占有する。
  // 閾値が変わったら true を返す (呼び出し側が反映する)。
  bool update(uint32_t nowMs, const ImuSample *sample);

 private:
  enum class Screen {
    Off,
    Menu,      // どの動作を扱うか
    Item,      // 測る / 有効無効 / 戻る
    Prepare,   // カウントダウン
    Record,    // 測定中
    Verdict,   // 測定結果 (OK / 理由)
    Test,      // 導出した閾値で試す
    Confirm,   // 保存するか
    Cleared,   // 全消去した
  };

  void show();
  void showMenu();
  void showItem();
  void showBig(const char *text, uint16_t color, const char *sub);

  void onShortPress();
  void onLongPress();

  void startRecording(uint32_t nowMs);
  void finishRecording();
  void startTest();
  void saveAndExit();

  const char *targetName(int index) const;

  CalibrationStore *store_ = nullptr;
  CalibrationSet *set_ = nullptr;

  Screen screen_ = Screen::Off;

  // メニューの選択位置。0-3 が動作、4 が RESET、5 が EXIT。
  int menuIndex_ = 0;
  static constexpr int kMenuCount = kCalibTargetCount + 2;
  static constexpr int kMenuReset = kCalibTargetCount;
  static constexpr int kMenuExit = kCalibTargetCount + 1;

  // 動作を選んだあとの操作。0 = 測る、1 = 有効無効、2 = 戻る。
  int itemIndex_ = 0;
  static constexpr int kItemCount = 3;

  Calibrator calibrator_;
  Calibrator::Result result_;

  // テスト段階で使う解析器。導出した閾値を入れて走らせる。
  MotionAnalyzer probe_;
  int hits_ = 0;

  uint32_t phaseStartMs_ = 0;
  int lastShownSecond_ = -1;
  bool dirty_ = false;
};

}  // namespace pet
