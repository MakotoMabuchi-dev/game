# RP2350 Touch LCD Game Collection

Waveshare `RP2350-Touch-LCD-1.85C` 向けのゲーム集プロジェクトです。

現在は以下を含みます。

- ゲーム選択用のランチャー画面
- `FAST`: カウントダウン後にランダムで赤く光り、反応速度を測るゲーム
- `HIT20`: 20 回タップするまでの時間を測るゲーム
- 共通の結果表示 UI
- タッチ入力、LCD 表示、短い WAV 音声の再生機能

## 対応ハードウェア

- ボード: Waveshare `RP2350-Touch-LCD-1.85C`
- MCU ターゲット: `pico2` / `rp2350-arm-s`
- ディスプレイ: ST77916
- タッチ: CST816
- オーディオ codec: ES8311

## ディレクトリ構成

```text
.
├── launcher.c            # アプリの開始点とゲーム選択処理
├── app.c / app.h         # 共通 UI、タッチ、音声、表示補助
├── games/
│   ├── games.c           # ゲーム一覧の登録
│   ├── 1_push_fast/      # 反応速度ゲーム
│   └── 2_hit20/          # 20 回タップゲーム
├── assets/
│   ├── 1_push_fast/      # ゲームごとの音声アセット
│   └── ui/               # 共通 SVG アイコンと生成済みヘッダ
├── bsp/                  # LCD、タッチ、音声、I2C、PIO まわり
└── tools/                # アセット変換用スクリプト
```

## ビルド

必要なもの:

- Raspberry Pi Pico SDK
- CMake
- ARM 向け組み込みツールチェーン
- `picotool`

最初に configure:

```sh
cmake -S . -B build
```

ビルド:

```sh
cmake --build build
```

生成される ELF:

```sh
build/test.elf
```

## 書き込み

`picotool` を使う場合の例:

```sh
picotool load build/test.elf -fx
```

## 現在のゲームの流れ

起動するとランチャーに `SELECT` が表示されます。

- 画面左端をタッチ: 前のゲーム
- 画面右端をタッチ: 次のゲーム
- 画面中央をタッチ: 選択中のゲームを開始

各ゲームは共通の結果画面を使っています。

- 上部にゲーム名
- 中央に今回の記録
- その下に過去最高記録
- 左にリプレイアイコン
- 右に終了アイコン

## 新しいゲームの追加方法

1. `games/` の下に新しいフォルダを作る。例: `games/3_new_game/`
2. `3_new_game.c` と `3_new_game.h` を追加する
3. `games/games.c` に登録する
4. `CMakeLists.txt` にソースを追加する
5. 必要なら `assets/3_new_game/` のようにゲーム専用アセットを追加する

ランチャーや結果画面は共通化してあるため、全体構成を大きく変えずにゲームを増やせます。

## アセット

UI アイコンは `assets/ui/` に SVG で置き、ビットマップ用ヘッダへ変換しています。

```sh
python3 tools/svg_icon_to_header.py assets/ui/result_icons.h \
  assets/ui/continue_replay.svg \
  assets/ui/finish_logout.svg \
  assets/ui/best_crown.svg
```

`1_push_fast` では、埋め込み再生用の WAV ヘッダも生成して使っています。

## メモ

- RP2350 の RAM には制約があるため、フレームバッファは 1 枚だけを共用しています。
- ビルド生成物は `.gitignore` で除外しています。
