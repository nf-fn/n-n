#!/usr/bin/env python3
"""AtomS3R のモーション録音を test/fixtures/*.csv に取り込む。

使い方:
    pio run -e record -t upload --upload-port /dev/cu.usbmodem101
    python3 tools/capture_fixtures.py /dev/cu.usbmodem101

画面の指示どおりに動かし、画面を押すたびに 1 動作ずつ録音される。
すべて録り終えると #DONE が来てスクリプトが終了する。

ファームウェアの出力形式:
    #BEGIN <name> <durationMs>
    t,ax,ay,az,gx,gy,gz
    <行...>
    #END <name>
"""

import os
import sys
import time

try:
    import serial
except ImportError:
    sys.exit(
        "pyserial が要ります。PlatformIO の python で実行してください:\n"
        "  ~/.platformio/penv/bin/python tools/capture_fixtures.py <port>"
    )

FIXTURE_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "test", "fixtures"
)


def open_port(port, attempts=40):
    """リセット直後は USB CDC の再列挙を待つ必要がある。"""
    for attempt in range(attempts):
        try:
            return serial.Serial(port, 115200, timeout=0.5)
        except Exception as exc:  # noqa: BLE001
            if attempt == attempts - 1:
                sys.exit(f"ポートを開けませんでした: {exc}")
            time.sleep(0.25)
    return None


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem101"
    os.makedirs(FIXTURE_DIR, exist_ok=True)

    ser = open_port(port)
    print(f"接続しました: {port}")
    print("画面の指示に従って動かしてください。Ctrl-C で中断できます。\n")

    current_name = None
    rows = []
    written = []

    with ser:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue

            if line.startswith("#MOTION "):
                _, name, guide = line.split(" ", 2)
                print(f"  {name:8s} {guide}")
                continue

            if line.startswith("#PREP "):
                parts = line.split(" ", 2)
                print(f"\n[準備] {parts[1]}: {parts[2] if len(parts) > 2 else ''}")
                continue

            if line.startswith("#BEGIN "):
                parts = line.split()
                current_name = parts[1]
                rows = []
                print(f"[録音中] {current_name}")
                continue

            if line.startswith("#END "):
                name = line.split()[1]
                path = os.path.join(FIXTURE_DIR, f"{name.lower()}.csv")
                with open(path, "w", encoding="utf-8") as f:
                    f.write("t,ax,ay,az,gx,gy,gz\n")
                    f.write("\n".join(rows))
                    f.write("\n")
                print(f"[保存] {path} ({len(rows)} サンプル)")
                written.append((name, len(rows)))
                current_name = None
                continue

            if line.startswith("#DONE"):
                print("\nすべて録り終えました:")
                for name, count in written:
                    print(f"  {name:8s} {count:5d} サンプル")
                return

            if line.startswith("#"):
                continue

            # ヘッダ行は保存側で書き直すので捨てる
            if line.startswith("t,"):
                continue

            if current_name is not None:
                rows.append(line)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n中断しました")
