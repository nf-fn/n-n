#include "CalibrationUi.h"

namespace pet {
namespace {

// 測定の長さ。動作ごとに必要な時間が違う。
constexpr uint32_t kPrepareMs = 3000;

uint32_t recordMsFor(CalibTarget t) {
  switch (t) {
    case CalibTarget::Tap: return 8000;    // 間をあけて数回つつく
    case CalibTarget::Stroke: return 8000; // 撫で続ける
    case CalibTarget::Shake: return 6000;
    case CalibTarget::Lift: return 10000;  // 持ち上げて置くをくり返す
  }
  return 8000;
}

const char *guideFor(CalibTarget t) {
  switch (t) {
    case CalibTarget::Tap: return "poke it";
    case CalibTarget::Stroke: return "stroke it";
    case CalibTarget::Shake: return "shake it";
    case CalibTarget::Lift: return "lift+place";
  }
  return "";
}

// テスト段階で数える対象
bool isHit(const MotionAnalyzer &a, CalibTarget t) {
  switch (t) {
    case CalibTarget::Tap: return a.event() == MotionEvent::Tap;
    case CalibTarget::Lift: return a.event() == MotionEvent::Lift;
    case CalibTarget::Stroke: return a.activity() == Activity::Stroke;
    case CalibTarget::Shake: return a.activity() == Activity::Shake;
  }
  return false;
}

}  // namespace

void CalibrationUi::begin(CalibrationStore *store, CalibrationSet *set) {
  store_ = store;
  set_ = set;
}

void CalibrationUi::enter() {
  screen_ = Screen::Menu;
  menuIndex_ = 0;
  show();
}

const char *CalibrationUi::targetName(int index) const {
  switch (index) {
    case 0: return "TAP";
    case 1: return "STROKE";
    case 2: return "SHAKE";
    case 3: return "LIFT";
    case kMenuReset: return "RESET";
    case kMenuExit: return "EXIT";
  }
  return "?";
}

void CalibrationUi::showBig(const char *text, uint16_t color, const char *sub) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(color);
  M5.Display.setTextSize(2);
  M5.Display.drawString(text, 64, 54);

  if (sub != nullptr) {
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_DARKGREY);
    M5.Display.drawString(sub, 64, 88);
  }
}

void CalibrationUi::showMenu() {
  const char *name = targetName(menuIndex_);

  const char *sub = "";
  if (menuIndex_ < kCalibTargetCount) {
    const MotionCalib &c =
        set_->at(static_cast<CalibTarget>(menuIndex_));
    // 未測定 / 測ったが無効 / 有効
    sub = !c.hasData ? "default" : (c.enabled ? "ON" : "OFF");
  }

  showBig(name, TFT_YELLOW, sub);

  char pos[16];
  snprintf(pos, sizeof(pos), "%d/%d", menuIndex_ + 1, kMenuCount);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY);
  M5.Display.drawString(pos, 64, 108);
}

void CalibrationUi::showItem() {
  const char *action = itemIndex_ == 0   ? "MEASURE"
                       : itemIndex_ == 1 ? "ON/OFF"
                                         : "BACK";
  showBig(action, TFT_CYAN, targetName(menuIndex_));
}

void CalibrationUi::show() {
  switch (screen_) {
    case Screen::Menu: showMenu(); break;
    case Screen::Item: showItem(); break;
    case Screen::Cleared: showBig("CLEARED", TFT_GREEN, nullptr); break;
    default: break;
  }
}

void CalibrationUi::startRecording(uint32_t nowMs) {
  calibrator_.begin(static_cast<CalibTarget>(menuIndex_));
  screen_ = Screen::Record;
  phaseStartMs_ = nowMs;
  lastShownSecond_ = -1;
}

void CalibrationUi::finishRecording() {
  result_ = calibrator_.finish();
  screen_ = Screen::Verdict;

  if (result_.ok) {
    showBig("OK", TFT_GREEN, "hold to test");
  } else {
    showBig("RETRY", TFT_RED, result_.reason);
  }
}

void CalibrationUi::startTest() {
  // 導出した値だけを有効にした閾値で試す。保存はまだしない。
  CalibrationSet trial = *set_;
  const CalibTarget t = static_cast<CalibTarget>(menuIndex_);
  trial.at(t) = result_.calib;
  trial.at(t).enabled = true;

  probe_ = MotionAnalyzer{};
  probe_.setThresholds(resolve(trial));
  hits_ = 0;

  screen_ = Screen::Test;
  lastShownSecond_ = -1;
  const bool discrete = (t == CalibTarget::Tap || t == CalibTarget::Lift);
  showBig(discrete ? "0" : "--", TFT_WHITE, guideFor(t));
}

void CalibrationUi::saveAndExit() {
  const CalibTarget t = static_cast<CalibTarget>(menuIndex_);
  set_->at(t) = result_.calib;
  set_->at(t).enabled = true;

  if (store_ != nullptr) {
    store_->save(*set_);
  }
  dirty_ = true;

  screen_ = Screen::Menu;
  show();
}

void CalibrationUi::onShortPress() {
  switch (screen_) {
    case Screen::Menu:
      menuIndex_ = (menuIndex_ + 1) % kMenuCount;
      show();
      break;

    case Screen::Item:
      itemIndex_ = (itemIndex_ + 1) % kItemCount;
      show();
      break;

    case Screen::Verdict:
      // 気に入らなければ測り直しへ戻る
      screen_ = Screen::Item;
      itemIndex_ = 0;
      show();
      break;

    case Screen::Test:
      // 試して納得できなければ破棄。既定値のまま。
      screen_ = Screen::Menu;
      show();
      break;

    case Screen::Cleared:
      screen_ = Screen::Menu;
      show();
      break;

    default:
      break;
  }
}

void CalibrationUi::onLongPress() {
  switch (screen_) {
    case Screen::Menu:
      if (menuIndex_ == kMenuExit) {
        screen_ = Screen::Off;
      } else if (menuIndex_ == kMenuReset) {
        *set_ = CalibrationSet{};
        if (store_ != nullptr) {
          store_->clear();
        }
        dirty_ = true;
        screen_ = Screen::Cleared;
        show();
      } else {
        screen_ = Screen::Item;
        itemIndex_ = 0;
        show();
      }
      break;

    case Screen::Item:
      if (itemIndex_ == 0) {
        screen_ = Screen::Prepare;
        phaseStartMs_ = 0;  // 次の update で始める
        lastShownSecond_ = -1;
      } else if (itemIndex_ == 1) {
        MotionCalib &c = set_->at(static_cast<CalibTarget>(menuIndex_));
        if (c.hasData) {
          c.enabled = !c.enabled;
          if (store_ != nullptr) {
            store_->save(*set_);
          }
          dirty_ = true;
        }
        screen_ = Screen::Menu;
        show();
      } else {
        screen_ = Screen::Menu;
        show();
      }
      break;

    case Screen::Verdict:
      if (result_.ok) {
        startTest();
      } else {
        screen_ = Screen::Menu;
        show();
      }
      break;

    case Screen::Test:
      saveAndExit();
      break;

    default:
      break;
  }
}

bool CalibrationUi::update(uint32_t nowMs, const ImuSample *sample) {
  if (screen_ == Screen::Off) {
    return false;
  }

  if (M5.BtnA.wasHold()) {
    onLongPress();
  } else if (M5.BtnA.wasClicked()) {
    onShortPress();
  }

  switch (screen_) {
    case Screen::Prepare: {
      if (phaseStartMs_ == 0) {
        phaseStartMs_ = nowMs;
      }
      const uint32_t elapsed = nowMs - phaseStartMs_;
      if (elapsed >= kPrepareMs) {
        startRecording(nowMs);
        break;
      }
      const int left = static_cast<int>((kPrepareMs - elapsed) / 1000) + 1;
      if (left != lastShownSecond_) {
        lastShownSecond_ = left;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", left);
        showBig(buf, TFT_ORANGE,
                guideFor(static_cast<CalibTarget>(menuIndex_)));
      }
      break;
    }

    case Screen::Record: {
      if (sample != nullptr) {
        calibrator_.feed(*sample);
      }
      const uint32_t total =
          recordMsFor(static_cast<CalibTarget>(menuIndex_));
      const uint32_t elapsed = nowMs - phaseStartMs_;
      if (elapsed >= total) {
        finishRecording();
        break;
      }
      const int left = static_cast<int>((total - elapsed) / 1000) + 1;
      if (left != lastShownSecond_) {
        lastShownSecond_ = left;
        char buf[16];
        snprintf(buf, sizeof(buf), "REC %d", left);
        showBig(buf, TFT_RED,
                guideFor(static_cast<CalibTarget>(menuIndex_)));
      }
      break;
    }

    case Screen::Test: {
      if (sample == nullptr) {
        break;
      }
      probe_.update(*sample);

      const CalibTarget t = static_cast<CalibTarget>(menuIndex_);
      const bool hit = isHit(probe_, t);

      // つつきと持ち上げは回数を数える。撫でと振りは継続する状態なので、
      // 今そう判定されているかどうかを出す。
      if (t == CalibTarget::Tap || t == CalibTarget::Lift) {
        if (hit) {
          ++hits_;
          char buf[16];
          snprintf(buf, sizeof(buf), "%d", hits_);
          showBig(buf, TFT_GREEN, "hold=save");
        }
      } else {
        const int state = hit ? 1 : 0;
        if (state != lastShownSecond_) {
          lastShownSecond_ = state;
          showBig(hit ? "YES" : "--", hit ? TFT_GREEN : TFT_DARKGREY,
                  "hold=save");
        }
      }
      break;
    }

    default:
      break;
  }

  const bool changed = dirty_;
  dirty_ = false;
  return changed;
}

}  // namespace pet
