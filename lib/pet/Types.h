// ポケットペットの純粋ロジック層で使う型。
//
// このディレクトリのコードは M5 のヘッダを一切 include しない。
// Mac 上 (env:native) でそのままコンパイル・テストできることが前提。
#pragma once

#include <cstdint>

namespace pet {

// IMU の 1 サンプル。
//
// 軸は画面座標に一致する (フェーズ 0 で実測済み):
//   +X = 画面の右   +Y = 画面の上   +Z = 画面の外向き(手前)
//
// accel の単位は g、gyro の単位は deg/s。
struct ImuSample {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float gx = 0.0f;
  float gy = 0.0f;
  float gz = 0.0f;
  uint32_t tMs = 0;
};

// 瞬間的な出来事。起きた更新で 1 回だけ返る。
enum class MotionEvent {
  None,
  Tap,   // 指先でつつかれた。静かな状態に鋭い衝撃が入る
  Lift,  // 机から持ち上げられた
};

// 継続している状態。毎更新で現在の分類が返る。
//
// Held は「動いているが撫でられてはいない」ことを表す。手に持たれている
// 状態がこれに当たる。撫でと線形加速度の大きさが重なる (撫で 0.069-0.089 /
// 手持ち 0.091-0.225) ため、誤って撫でと判定しないための受け皿として必要。
// 分離は角速度で行う (撫で 9-15 / 手持ち 28-55 deg/s)。
enum class Activity {
  Quiet,   // 置かれて静止している
  Stroke,  // 撫でられている
  Held,    // 手に持たれている
  Shake,   // 振られている
};

// 連続量としての姿勢。重力方向から求める。
//
// 静止時の加速度ベクトルは「世界の上方向」を機体座標で表したものになる。
// これを平滑化したものが up。
struct Posture {
  // 平滑化した「世界の上」方向の単位ベクトル (機体座標)
  float upX = 0.0f;
  float upY = 0.0f;
  float upZ = 1.0f;

  // 画面平面における「下り坂」の方向。目玉が転がる先。
  // 画面座標 (+X 右 / +Y 上) で、長さは 0..1。
  // 画面が水平なら 0、垂直なら 1 に近づく。
  float downX = 0.0f;
  float downY = 0.0f;

  bool faceUp = true;      // 画面が上を向いている (机に置いた状態)
  bool faceDown = false;   // 画面が下を向いている (伏せた状態。顔が見えない)
  bool inverted = false;   // 画面の上端が下を向いている (顔が上下逆さに見える)
};

// 気分。電源が入っているあいだだけ連続する短期的な状態で、保存はしない。
//
// 状態機械ではなくパラメータの集合として持つ。こうすると表情の中間表現が
// 自然に出る。各値は時間とともに基準値へ向かって減衰する。
struct MoodState {
  float arousal = 0.3f;     // 覚醒度 0..1。放置で下がり、刺激で上がる
  float valence = 0.0f;     // 機嫌 -1..1。撫でで上がる
  float dizziness = 0.0f;   // めまい 0..1。振られると溜まる
  float anger = 0.0f;       // 怒り 0..1。乱暴が続くと溜まり、冷めにくい
  float sleepiness = 0.0f;  // 眠気 0..1。静止が続くと上がる
};

// 顔の描画パラメータ。FaceComposer が組み立て、FaceRenderer が描く。
//
// 位置は顔座標で -1..1 に正規化する (+X 右 / +Y 上)。
// ピクセルへの変換は FaceRenderer の責務。
//
// この顔は口を持たない。感情はすべて目と眉で表す:
//   笑い = 目を瞑った上向きの弧 (eyeOpen を下げ eyeArch を上げる)
//   怒り = 眉を内側に下げる     (browAngle を上げる)
//   困り = 眉を内側に上げる     (browAngle を下げる)
struct FaceParams {
  float eyeOffsetX = 0.0f;  // 目玉のずれ -1..1
  float eyeOffsetY = 0.0f;
  float eyeOpen = 1.0f;     // 0 = 閉じ、1 = 全開

  // 目を閉じたときの弧の向き。
  // -1 = 眠そうに下がる ∪ / 0 = まばたきの水平に近い形 / +1 = 笑いの ∩
  float eyeArch = 0.0f;

  // 眉。0 なら眉そのものを描かない (素の顔には眉が無い)。
  // -1 = 困り (内側が上がる) / +1 = 怒り (内側が下がる)
  float browAngle = 0.0f;

  // 瞳の大きさの倍率。興奮すると開き、落ち着くと縮む。
  // 1.0 が既定。生き物は感情が昂ると瞳孔が開くので、その代わり。
  float irisScale = 1.0f;

  float faceTiltRad = 0.0f;  // 顔全体の傾き
};

}  // namespace pet
