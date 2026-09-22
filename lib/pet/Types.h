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

// 顔の描画パラメータ。FaceComposer が組み立て、FaceRenderer が描く。
//
// 位置は顔座標で -1..1 に正規化する (+X 右 / +Y 上)。
// ピクセルへの変換は FaceRenderer の責務。
struct FaceParams {
  float eyeOffsetX = 0.0f;  // 目玉のずれ -1..1
  float eyeOffsetY = 0.0f;
  float eyeOpen = 1.0f;     // 0 = 閉じ、1 = 全開
  float mouthCurve = 0.0f;  // -1 = への字、0 = 一文字、1 = 笑い
  float faceTiltRad = 0.0f; // 顔全体の傾き
};

}  // namespace pet
