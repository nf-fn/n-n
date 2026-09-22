#include "CalibrationStore.h"

#include <Preferences.h>

namespace pet {
namespace {

constexpr const char *kNamespace = "pocketpet";
constexpr const char *kKeyVersion = "calver";
constexpr const char *kKeyData = "calib";

}  // namespace

bool CalibrationStore::load(CalibrationSet &out) {
  out = CalibrationSet{};

  // 読むだけだが読み書きで開く。読み取り専用で開くと、名前空間が無い初回に
  // NVS が NOT_FOUND をエラーログに出す。実際は既定値で動くので問題ないが、
  // 起動のたびに障害のように見えるのを避ける。
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return true;  // まだ何も保存されていない
  }

  const uint8_t version = prefs.getUChar(kKeyVersion, 0);
  if (version != kFormatVersion) {
    // 形式が違う。読まずに既定値のままにする。
    prefs.end();
    return true;
  }

  CalibrationSet stored;
  const size_t read = prefs.getBytes(kKeyData, &stored, sizeof(stored));
  prefs.end();

  if (read != sizeof(stored)) {
    return true;
  }

  out = stored;
  return true;
}

bool CalibrationStore::save(const CalibrationSet &set) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return false;
  }
  prefs.putUChar(kKeyVersion, kFormatVersion);
  const size_t written = prefs.putBytes(kKeyData, &set, sizeof(set));
  prefs.end();
  return written == sizeof(set);
}

bool CalibrationStore::clear() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return false;
  }
  const bool ok = prefs.clear();
  prefs.end();
  return ok;
}

}  // namespace pet
