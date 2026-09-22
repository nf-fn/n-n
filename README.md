# ポケットペット

M5Stack AtomS3R を手のひらサイズの生き物にするおもちゃ。
撫でる・振る・つつく・傾けるといった触り方に、顔と鳴き声で反応する。

- 設計: [docs/superpowers/specs/2026-09-22-pocket-pet-design.md](docs/superpowers/specs/2026-09-22-pocket-pet-design.md)
- 計画: [docs/superpowers/plans/2026-09-22-pocket-pet-plan.md](docs/superpowers/plans/2026-09-22-pocket-pet-plan.md)

## 必要なもの

- M5Stack AtomS3R
- M5Stack Atomic Echo Base（鳴き声を出すのに必要。無くても顔は動く）

## 遊び方

| やること | 反応 |
|---|---|
| 傾ける | 瞳が下り坂の方向へ転がる |
| つつく | 瞳が開いて短く鳴く |
| 持ち上げる | はっと目を開けて鳴く |
| 撫で続ける | 機嫌が上がり、目を瞑って笑う |
| 振り続ける | 瞳が回る。続けると眉を出して怒る |
| 1 分ほど放置 | うとうとして寝息を立てる |
| 逆さにする | 困り眉になる |
| 画面を伏せて置く | 5 秒ほどで寝かしつけられる |
| 画面を押す | つつくのと同じ |
| 画面を長押し | キャリブレーションに入る |

気分は電源が入っているあいだだけ続く。保存はしない。

## ビルドと書き込み

PlatformIO Core が PATH に無い場合は `~/.platformio/penv/bin/pio` にある。

```sh
# 実機へ書き込む
pio run -e atoms3r -t upload --upload-port /dev/cu.usbmodem101

# 純粋ロジック層のテストを Mac 上で走らせる (実機不要)
pio test -e native
```

AtomS3R は USB JTAG (VID:PID 303A:1001) として `/dev/cu.usbmodem*` に出る。

## キャリブレーション

既定の閾値は 1 人が 1 回録音しただけの値で、触り方の癖には合っていない。
実機だけで測り直せる。ボタンが 1 つなので **短押し = 次へ / 長押し = 決定**。

1. 画面を長押しして設定に入る
2. `TAP` / `STROKE` / `SHAKE` / `LIFT` / `RESET` / `EXIT` を短押しで送る
   - 動作名の下に `default`（未測定）/ `ON`（有効）/ `OFF`（測ったが無効）
3. 動作を長押しで選び、`MEASURE` を長押し
4. 3 秒のカウントダウンのあと、`REC n` の間にその動作をする
5. `OK` なら長押しでテストへ。`RETRY` なら理由が出る
6. テスト画面で実際に検出が走る。つつき・持ち上げは回数、撫で・振りは
   判定中に `YES`
7. 納得したら長押しで保存、気に入らなければ短押しで破棄

保存は NVS。`RESET` ですべて既定値に戻る。

測定が明らかにおかしい場合（撫での測定中に振ってしまった等）は妥当性検査で
弾かれ、保存できない。導出した値は範囲にも収めてあるので、極端な値が
焼き付いて検出が死ぬことはない。

## モーションの録音

検出の閾値は実測から決めている。録り直す場合:

```sh
pio run -e record -t upload --upload-port /dev/cu.usbmodem101
~/.platformio/penv/bin/python tools/capture_fixtures.py /dev/cu.usbmodem101
```

画面の指示どおりに動かすと `test/fixtures/*.csv` が更新される。

## 構成

ハードに触る層と、触らない純粋ロジック層を分けてある。
おもちゃの面白さは閾値と気分の遷移カーブの調整で決まるので、そこを
実機への書き込みなしで回せることを優先した。

```
lib/pet/   純粋ロジック。M5 に依存せず Mac 上でテストできる
  MotionAnalyzer  IMU の解釈 (姿勢・つつき・持ち上げ・撫で/手持ち/振り)
  Mood            気分の蓄積と減衰
  FaceComposer    気分 → 顔のパラメータ
  Voice           気分 → 音符の並び
  VoiceSynth      音符の並び → PCM
  Calibration     測定 → 閾値の導出と反映
  Thresholds      検出の閾値

src/       I/O。M5Unified に依存する
  ImuSource       IMU を読む
  FaceRenderer    顔を描く
  VoiceOutput     PCM を鳴らす
  CalibrationStore  測定結果を NVS に保存
  CalibrationUi     ボタン 1 つの設定画面
  main.cpp        繋ぎ込み
  record_main.cpp 録音用ファームウェア (env:record)

test/      native テスト。fixtures/ は実機で録ったモーション
```
