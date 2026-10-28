# Game Boy Emulator

This is a Game Boy and Game Boy Color emulator written in C++17 with an SDL2 and Dear ImGui desktop frontend.
The project includes CPU, memory, cartridge, timer, graphics, input, audio,
and Super Game Boy components.

## Features

- Game Boy and Game Boy Color cartridge support
- Cartridge controllers: ROM-only, MBC1, MBC2, MBC3, and MBC5
- SDL-rendered display and keyboard controls
- Dear ImGui menu bar with a built-in file browser for opening and closing ROMs
- Stereo audio output through SDL
- Battery-backed cartridge RAM saved beside the ROM
- Optional Game Boy Color boot ROM

Emulation coverage is still evolving, so compatibility varies by game.

## Screenshots

| Pokémon Crystal | Pokémon Crystal |
| --- | --- |
| ![Pokémon Crystal gameplay](images/pokemon_crystal_1.png) | ![Pokémon Crystal gameplay](images/pokemon_crystal_2.png) |

| Super Mario Bros. Deluxe | Super Mario Bros. Deluxe |
| --- | --- |
| ![Super Mario Bros. Deluxe gameplay](images/smbd_1.png) | ![Super Mario Bros. Deluxe gameplay](images/smbd_2.png) |

| Super Mario Land 2 | Super Mario Land 2 |
| --- | --- |
| ![Super Mario Land 2 gameplay](images/sml2_1.png) | ![Super Mario Land 2 gameplay](images/sml2_2.png) |

## Requirements

- C++17-compatible compiler: GCC 9 or newer, Clang 9 or newer
- CMake 3.23 or newer
- SDL2 development package with CMake package configuration
- [Dear ImGui](https://github.com/ocornut/imgui), included as a git submodule in `external/imgui`

## Build

Clone with submodules, or initialize them in an existing checkout:

```sh
git clone --recursive <repository-url>
# or, inside an existing clone:
git submodule update --init --recursive
```

Then, from the repository root:

```sh
cmake -S . -B build
cmake --build build
```

The executable is created at `build/gbemu` (on Windows, it may be `build/Debug/gbemu.exe`
or `build/Release/gbemu.exe`, depending on the generator).

## Run

```sh
./build/gbemu
```

Use **File > Open ROM...** to browse for a `.gb`, `.gbc`, or `.sgb` file. Double-click
a folder to enter it, or type a path into the bar at the top of the browser and press Enter.
Double-click a ROM or select it and click **Open** to start it. The game pauses while
the browser is open. **File > Close ROM** saves battery-backed RAM and stops the game.

A ROM and an optional Game Boy Color boot ROM can still be passed on the command line:

```sh
./build/gbemu path/to/game.gbc path/to/cgb_boot.bin
```

### Keyboard controls

| Game Boy button | Keyboard |
| --- | --- |
| D-pad | W, A, S, D |
| A | P |
| B | O |
| Select | Backspace |
| Start | Enter |

When the cartridge has battery-backed RAM, the emulator stores it in a
`.battery` file next to the ROM. For example, `game.gb` uses `game.gb.battery`.

## Project layout

- `src/` — emulator core and SDL/ImGui application
- `external/imgui/` — Dear ImGui submodule
- `CMakeLists.txt` — build configuration

## Contributing

Contributions and bug reports are welcome. Open an issue to describe a
compatibility problem or proposed change, or submit a pull
request with a focused improvement.
