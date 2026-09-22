# ポケットペット実装計画

**仕様:** [docs/superpowers/specs/2026-09-22-pocket-pet-design.md](../specs/2026-09-22-pocket-pet-design.md)
**日付:** 2026-09-22

## 進め方

純粋ロジック層はテストを先に書く。`env:native` で Mac 上から回すため、
実機への書き込みを挟まずに反復できる。

I/O 層 (`ImuSource` / `FaceRenderer`) はテストせず、実機で目視確認する。

各フェーズの末尾に実機で触るチェックポイントを置く。そこで手応えを確かめてから
次へ進む。特にフェーズ 1 の後は、面白くなければ設計に戻る。

## ディレクトリ構成

```
platformio.ini
src/
  main.cpp              loop、各層の接続
  ImuSource.cpp/.h      [I/O] M5.Imu → ImuSample、軸補正
  FaceRenderer.cpp/.h   [I/O] FaceParams → M5Canvas 描画
  Recorder.cpp/.h       [I/O] デバッグ時の CSV シリアル出力
lib/
  pet/
    Types.h             ImuSample, Posture, MotionEvent, MoodState, FaceParams
    MotionAnalyzer.cpp/.h
    Mood.cpp/.h
    FaceComposer.cpp/.h
test/
  fixtures/*.csv        実機録画したモーション
  test_native/
    test_motion_analyzer.cpp
    test_mood.cpp
    test_face_composer.cpp
    fixture_loader.cpp/.h
```

`lib/pet/` は M5 のヘッダを一切 include しない。これが native テストの前提になる。

---

## フェーズ 0: 環境と実機確認 — 完了 (2026-09-22)

### 実施したこと

- `platformio.ini` に `env:atoms3r` と `env:native` を定義
- 確認スケッチを実機に書き込み、姿勢ごとの加速度を実測

### 結果

- IMU は **BMI270**、M5Unified 0.2.7 が認識。専用ドライバ不要
- **PSRAM 8MB 有効**、Flash 8MB、画面 128×128
- 軸は画面座標と一致（**+X 右 / +Y 上 / +Z 手前**）。
  **`ImuSource` での軸補正は不要**
- RGB LED は未確認のまま、使わない方針とした

詳細は仕様書の「6. 実機確認の結果」を参照。

---

## フェーズ 1: 縦の串を通す（マイルストーン 1）

傾けると目玉が転がる。これだけでおもちゃとして成立させる。

イベント検出も気分もまだ無い。姿勢だけ。

### 1.1 型定義 (`lib/pet/Types.h`)

`ImuSample` / `Posture` / `FaceParams` を定義する。
`MotionEvent` と `MoodState` はこの段階では書かない。

### 1.2 `MotionAnalyzer` の姿勢算出（テスト先行）

テスト: 重力ベクトルを与えると期待する `Posture` が返る。

- 画面が真上 → 傾き 0
- 右に 30 度 → roll 約 30 度
- 逆さ → `upsideDown` が true
- 振っている最中の大きな加速度 → 姿勢が暴れないこと（ローパスが効いている）

最後の 1 本が重要。生の加速度をそのまま使うと、動かした瞬間に顔が壊れる。

### 1.3 `FaceComposer` の傾き反応（テスト先行）

テスト: `Posture` から `FaceParams` へ。

- 傾き 0 → 目は中央
- 右に傾ける → 目玉が右下に寄る
- 目玉が目の輪郭からはみ出さない（境界のクランプ）

### 1.4 `FaceRenderer` (I/O)

`M5Canvas` に 128×128 を描いて一括転送。目・口・輪郭を `FaceParams` から描く。
まばたきもここで入れる（一定間隔 + ランダム揺らぎ）。まばたきがあるだけで
生き物らしさが跳ね上がるので、早い段階で入れる。

### 1.5 `ImuSource` (I/O) と `main.cpp`

フェーズ 0 で決めた軸補正を適用する。センサ 100Hz、描画 30fps でループを組む。

### チェックポイント（実機）

本体を持って傾ける。目玉が滑らかに追従するか。カクつき、ちらつき、
目玉のはみ出しがないか。**ここで「かわいい」と思えなければ先に進まない。**

---

## フェーズ 2: フィクスチャ基盤とイベント検出

### 2.1 `Recorder` と録画

デバッグビルド時のみ、`ImuSample` を CSV でシリアルに吐く。
Mac 側で受けてファイルに落とす手順を README に書く。

実機で以下を録り、`test/fixtures/` に保存する。

`stroke.csv` 撫でる / `shake.csv` 振る / `tap.csv` 叩く /
`lift.csv` 持ち上げる / `spin.csv` 回す / `idle.csv` 放置 /
`walk_pocket.csv` ポケットに入れて歩く

`walk_pocket.csv` は**誤検出を防ぐための負例**として効く。歩いているだけで
撫でられたと判定されると、持ち歩くおもちゃとして成立しない。

### 2.2 フィクスチャローダ

CSV を `std::vector<ImuSample>` にする native テスト用ヘルパ。

### 2.3 イベント検出（テスト先行、1 つずつ）

各イベントについて「正例で発火する」と「他の全フィクスチャで発火しない」の
両方をテストする。後者を毎回書くことで、閾値の調整が他を壊さないことを保証する。

順序: 叩く（一番特徴が鋭い） → 振る → 持ち上げる → 回す → 撫でる（一番難しい）

撫でるの判定は `walk_pocket.csv` と衝突しやすい。周期性と振幅の両方で分ける。

### チェックポイント（実機）

各動作をして、シリアルに正しいイベント名が出るか。
狙っていない動作で誤発火しないか。誤発火したらその動作を録って
フィクスチャに追加し、負例として固定する。

---

## フェーズ 3: 気分

### 3.1 `MoodState` と減衰（テスト先行）

`arousal` / `valence` / `dizziness` / `anger` / `sleepiness` を時間減衰する
連続量として持つ。

テスト:

- 叩かれると `arousal` が跳ね、時間で戻る
- 振られ続けると `dizziness` が溜まり、止めると抜ける
- 乱暴が続くと `anger` が閾値を超える
- 撫でると `anger` が下がり `valence` が上がる
- 何もしなければ全ての値が中立に収束する（暴走しない）

最後の 1 本で、値が発散しないことを保証する。

### 3.2 `FaceComposer` の拡張（テスト先行）

`MoodState` を表情に落とす。代表的な気分に対する `FaceParams` を検証する。
表情の切り替わりが不連続にならないこと（パラメータの補間）も確認する。

### チェックポイント（実機）

振ってみて目が回るか。撫でて落ち着くか。**気分の減衰速度をここで詰める。**
速すぎると無反応に感じ、遅すぎるといつまでも怒っている。

---

## フェーズ 4: 放置と演出

- 眠気 → うとうと → 寝息のアニメーション
- 画面ボタンで起こす
- 逆さまの困り顔、回転への反応
- アイドル時のフレームレート低下（バッテリ駆動への布石）

### チェックポイント（実機）

机に置いて放置し、自然に眠るか。触って起きるか。
しばらく一緒に過ごして、飽きないか。

---

## 各フェーズの独立性

フェーズ 1〜4 は前のフェーズに依存するため、順番に実施する。
並列化の余地はフェーズ 2.3 のイベント検出のみだが、閾値が互いに干渉するため
逐次に進めるほうが安全。

## 開発環境のメモ

PlatformIO Core は PATH に無く、`~/.platformio/penv/bin/pio` にある。

```sh
~/.platformio/penv/bin/pio run -e atoms3r -t upload --upload-port /dev/cu.usbmodem101
~/.platformio/penv/bin/pio test -e native
```

AtomS3R は USB JTAG (VID:PID 303A:1001) として `/dev/cu.usbmodem101` に出る。
