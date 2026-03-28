# RP2350 Touch LCD Game Collection

Game collection for the Waveshare `RP2350-Touch-LCD-1.85C`.

This project currently includes:

- A launcher screen with game selection
- `FAST`: reaction game with countdown, random red flash, touch timing, and best score
- `HIT20`: 20-tap speed game with best score
- Shared result screen UI
- Touch input, LCD output, and short WAV playback support

## Hardware

- Board: Waveshare `RP2350-Touch-LCD-1.85C`
- MCU target: `pico2` / `rp2350-arm-s`
- Display: ST77916
- Touch: CST816
- Audio codec: ES8311

## Project Layout

```text
.
├── launcher.c            # app entry point and game selection flow
├── app.c / app.h         # shared UI, touch, audio, formatting helpers
├── games/
│   ├── games.c           # game registry
│   ├── 1_push_fast/      # reaction game
│   └── 2_hit20/          # 20-tap game
├── assets/
│   ├── 1_push_fast/      # per-game audio assets
│   └── ui/               # shared SVG icons and generated icon header
├── bsp/                  # LCD, touch, audio, I2C, PIO support code
└── tools/                # asset conversion helpers
```

## Build

Requirements:

- Raspberry Pi Pico SDK
- CMake
- A working ARM embedded toolchain
- `picotool`

First configure:

```sh
cmake -S . -B build
```

Build:

```sh
cmake --build build
```

The output ELF is:

```sh
build/test.elf
```

## Flash

Example with `picotool`:

```sh
picotool load build/test.elf -fx
```

## Current Game Flow

On boot, the launcher shows `SELECT`.

- Tap left edge: previous game
- Tap right edge: next game
- Tap center: start selected game

Each game uses the same result screen layout:

- Game title at the top
- Current record in the center
- Best record below it
- Replay icon on the left
- Finish icon on the right

## Adding a New Game

1. Create a new folder under `games/`, for example `games/3_new_game/`
2. Add `3_new_game.c` and `3_new_game.h`
3. Register it in `games/games.c`
4. Add the source file to `CMakeLists.txt`
5. If needed, add per-game assets under `assets/3_new_game/`

The shared launcher and result screen are designed so additional games can be added without changing the overall app structure.

## Assets

UI icons are stored as SVG files in `assets/ui/` and converted into a bitmap header:

```sh
python3 tools/svg_icon_to_header.py assets/ui/result_icons.h \
  assets/ui/continue_replay.svg \
  assets/ui/finish_logout.svg \
  assets/ui/best_crown.svg
```

`1_push_fast` also includes a generated WAV header for embedded playback.

## Notes

- RAM is limited on RP2350, so the project keeps a single shared framebuffer.
- Build artifacts are ignored by Git via `.gitignore`.
