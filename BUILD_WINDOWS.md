# Building gbemu on Windows

gbemu builds on Windows in one of two ways:

- [MSVC](#msvc-visual-studio-2026), with Visual Studio 2026. Qt comes from Visual Studio's vcpkg
  and stays inside the repository, so nothing is installed system-wide.
- [MSYS2 UCRT64](#msys2-ucrt64), with GCC or Clang and MSYS2's Qt.

Both build for 64-bit x86 Windows (x64). gbemu builds in Release, which is optimized for playing
games, or Debug, which is for debugging the emulator. CMake's other two build types, RelWithDebInfo
and MinSizeRel, work too.

Contents:

- [MSVC (Visual Studio 2026)](#msvc-visual-studio-2026)
  - [Requirements](#requirements)
  - [How Qt is installed](#how-qt-is-installed)
  - [Build](#build)
  - [Other build types](#other-build-types)
  - [Presets](#presets)
  - [Build in Visual Studio](#build-in-visual-studio)
  - [Build with Ninja](#build-with-ninja)
  - [Use a different vcpkg](#use-a-different-vcpkg)
  - [Compiler flags](#compiler-flags)
  - [Run](#run)
  - [Updating and rebuilding](#updating-and-rebuilding)
  - [Troubleshooting](#troubleshooting)
- [MSYS2 UCRT64](#msys2-ucrt64)
  - [Requirements](#requirements-1)
  - [Release build](#release-build)
  - [Debug build](#debug-build)
  - [Other build types](#other-build-types-1)
  - [Build with Clang](#build-with-clang)
  - [Compiler flags](#compiler-flags-1)
  - [Run](#run-1)
  - [Updating and rebuilding](#updating-and-rebuilding-1)
  - [Troubleshooting](#troubleshooting-1)

## MSVC (Visual Studio 2026)

### Requirements

- Windows on an x64 PC
- Visual Studio 2026 with the **Desktop development with C++** workload. It includes MSVC, CMake,
  Ninja and vcpkg.
- Git
- PowerShell 7. Windows PowerShell 5.1, which comes with Windows, isn't enough.

Install Git and PowerShell 7 with winget:

```powershell
winget install --id Git.Git --source winget
winget install --id Microsoft.PowerShell --source winget --installer-type wix
```

`--installer-type wix` installs PowerShell 7 from its MSI, in `C:\Program Files\PowerShell\7`.
vcpkg can't run the Microsoft Store version, so uninstall that one if you have it. Open a new
terminal after installing, so the tools are on the path.

The commands in this section need CMake on the path. The **Developer PowerShell for VS 2026**, in
the Start menu, puts Visual Studio's CMake on the path. To use any other terminal, install CMake
system-wide too:

```powershell
winget install --id Kitware.CMake --source winget
```

### How Qt is installed

When CMake configures gbemu outside MSYS2, it finds Visual Studio's vcpkg (through `VCPKG_ROOT`,
or through `vswhere` if that isn't set). vcpkg then builds Qt from source, as listed in
`vcpkg.json`. It uses the CMake, Ninja, Git and PowerShell 7 already installed instead of
downloading its own.

Everything vcpkg makes stays in the `.vcpkg/` folder of the repository, not in your user profile:

- `.vcpkg/downloads`: source archives, and the Perl, Python and pkg-config that Qt's build
  downloads
- `.vcpkg/buildtrees` and `.vcpkg/packages`: vcpkg's work while building Qt
- `.vcpkg/archives`: built packages, reused instead of building them again
- `.vcpkg/installed-x64-windows`: Qt with Debug and Release
- `.vcpkg/installed-x64-windows-release`: Qt with only Release

Building Qt takes a while the first time. Later configures reuse it. Delete `.vcpkg/` to remove it
all.

### Build

From the repository root, first configure the `build` folder. Choose what the folder builds.

To build both Release and Debug, configure with no option. vcpkg builds a Debug and a Release Qt
for it:

```powershell
cmake -B build
```

To build only Release, add `-DCMAKE_BUILD_TYPE=Release`. vcpkg builds only a Release Qt, so the
first configure is faster and `.vcpkg/` is smaller:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

To build only Debug, add `-DCMAKE_BUILD_TYPE=Debug`. vcpkg can't build Qt only for Debug, so it
still builds a Debug and a Release Qt:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

Then build, choosing the build type with `--config`. To build Release, which creates
`build/Release/gbemu.exe`:

```powershell
cmake --build build --config Release --parallel
```

To build Debug, which creates `build/Debug/gbemu.exe`:

```powershell
cmake --build build --config Debug --parallel
```

A folder configured for both can run either command, and the Release and Debug builds don't
replace each other. A folder configured for only Release or only Debug needs `--config` with that
build type. Without `--config`, `cmake --build` builds Debug, which a Release-only folder can't
build.

`--config` only works on `cmake --build`. `cmake -B` doesn't take it: use `-DCMAKE_BUILD_TYPE`
there instead.

Every command in this section configures and builds the `build` folder, with the options for that
build on `cmake -B build`. A build folder keeps the options it was first configured with, so to
change them on a folder you've already configured, add `--fresh` to `cmake -B build`:

```powershell
cmake -B build --fresh -DCMAKE_BUILD_TYPE=Release
```

To keep several builds side by side, use another folder name in both commands instead of `build`,
such as `build-debug`.

### Other build types

CMake has two more build types, which both use the Release Qt:

- **RelWithDebInfo** is optimized like Release, and also has debug information, for debugging at
  full speed.
- **MinSizeRel** is optimized for a smaller executable.

A folder configured with no option can build them too, with `--config`:

```powershell
cmake --build build --config RelWithDebInfo --parallel
```

This creates `build/RelWithDebInfo/gbemu.exe`. To make the folder build only one of them, and get
only a Release Qt, configure it with `-DCMAKE_BUILD_TYPE`:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --config RelWithDebInfo --parallel
```

### Presets

`CMakePresets.json` has presets for a Release-only and a Debug-only folder. To build Release in
`build-release`:

```powershell
cmake --preset msvc-release
cmake --build --preset msvc-release --parallel
```

To build Debug in `build-debug`:

```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug --parallel
```

They work the same as `cmake -B` with `-DCMAKE_BUILD_TYPE`, and create
`build-release/Release/gbemu.exe` and `build-debug/Debug/gbemu.exe`. Unlike the commands above, the
presets use their own folders, so Visual Studio can switch between them without starting a folder
over. The presets need CMake 3.25 or
newer, which Visual Studio 2026 includes.

### Build in Visual Studio

Visual Studio can build gbemu in two ways.

To work from the repository folder, open it with **File > Open > Folder**. Visual Studio reads
`CMakePresets.json` and offers **MSVC Release** and **MSVC Debug** in the configuration list on the
toolbar. Choose one, and choose **gbemu.exe** in the startup item list next to it. Then build with
**Build > Build All**, and start gbemu with **Debug > Start Debugging** (F5).

To work from a Visual Studio solution, configure a folder with `cmake -B build` first. CMake creates
a solution for gbemu in the `build` folder. Open it in Visual Studio and choose **Release** or
**Debug** in the configuration list on the toolbar. **Build > Build Solution** builds gbemu, and
**Debug > Start Debugging** (F5) builds and starts it.

### Build with Ninja

[Ninja](https://ninja-build.org/) builds faster than the Visual Studio generator. Ninja and MSVC
need the Visual Studio environment, so run these commands from a **Developer PowerShell for VS
2026**:

```powershell
cmake -B build -G Ninja
cmake --build build
```

A Ninja folder holds one build type. Release is the default, and the executable is created at
`build/gbemu.exe`. For a Debug build, add `-DCMAKE_BUILD_TYPE=Debug`:

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

If `build` was configured with the Visual Studio generator before, add `--fresh`: a folder can't
change generators otherwise.

Ninja builds in parallel by default, so `--parallel` and `--config` aren't needed. A Ninja folder
also contains `compile_commands.json`, which editors such as Visual Studio Code (with clangd) use
for code completion and navigation. To use Ninja Multi-Config instead, which holds every build type
in one folder like the Visual Studio generator, configure with `-G "Ninja Multi-Config"` and build
with `--config`.

### Use a different vcpkg

gbemu uses Visual Studio's vcpkg by default. To use another copy of vcpkg, such as a clone of
[the vcpkg repository](https://github.com/microsoft/vcpkg), set `VCPKG_ROOT` to its folder before
configuring `build`, and add `--fresh` if `build` was configured before. Run this from a **Developer PowerShell for VS 2026**, so Ninja,
which vcpkg uses to build Qt, is on the path:

```powershell
$env:VCPKG_ROOT = "C:\src\vcpkg"
cmake -B build
```

Passing vcpkg's toolchain file directly works the same way:

```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:\src\vcpkg\scripts\buildsystems\vcpkg.cmake"
```

Everything this vcpkg downloads, builds and installs for gbemu still stays in `.vcpkg/`.

### Compiler flags

- Release: `/O2`
- Debug: unoptimized (`/Od`), with debug information, so it may run slower than full speed
- RelWithDebInfo: `/O2` with debug information
- MinSizeRel: `/O1`, optimized for size

Every build type reads the sources as UTF-8 (`/utf-8`) and follows the C++ standard strictly
(`/permissive-`).

### Run

Each build has the Qt DLLs and plugins it needs copied next to it (by vcpkg and `windeployqt`), so
it runs from any terminal, or by double-clicking it in File Explorer:

```powershell
.\build\Release\gbemu.exe
```

To start a game straight away, pass the ROM's path:

```powershell
.\build\Release\gbemu.exe path\to\game.gbc
```

gbemu runs from its build folder and doesn't need to be installed. See [Run](README.md#run) in the
README for how to open games, set up the BIOS files, and change the settings.

### Updating and rebuilding

To update to the latest source code, pull the changes and build again. CMake rebuilds only what
changed:

```powershell
git pull
cmake --build build --config Release --parallel
```

When an update changes `vcpkg.json`, the next build reconfigures the folder, and vcpkg builds the
changed Qt packages.

To start a build folder over, delete it and configure it again:

```powershell
Remove-Item -Recurse -Force build
cmake -B build
```

To build Qt again from the start, also delete `.vcpkg/`.

### Troubleshooting

#### "Visual Studio's vcpkg wasn't found"

CMake found neither `VCPKG_ROOT` nor a Visual Studio installation with vcpkg. Install Visual Studio
2026 with the **Desktop development with C++** workload, which includes vcpkg. If Visual Studio is
installed, open the **Visual Studio Installer**, choose **Modify**, and make sure the workload is
selected.

#### "vcpkg needs PowerShell 7 (pwsh), which isn't installed"

Install PowerShell 7 from its MSI, as in [Requirements](#requirements), and open a new terminal. If
the Microsoft Store version is installed, uninstall it first: vcpkg can't run it.

#### "vcpkg needs Git, which isn't installed"

Install Git, as in [Requirements](#requirements), and open a new terminal.

#### "vcpkg needs Ninja"

CMake looks for Ninja next to Visual Studio's CMake, then on the path. Open the **Visual Studio
Installer**, choose **Modify**, and make sure **C++ CMake tools for Windows** is selected under the
**Desktop development with C++** workload. With a [different vcpkg](#use-a-different-vcpkg),
configure from a **Developer PowerShell for VS 2026**.

#### "This build folder was configured with vcpkg's ... Qt, but this build type needs ..."

The folder was first configured for a different build type. Add `--fresh` to start it over, or
configure another folder:

```powershell
cmake -B build --fresh -DCMAKE_BUILD_TYPE=Release
```

#### "This project doesn't contain the Configuration and Platform combination of Debug|x64"

The folder builds only Release, and `cmake --build` built Debug because `--config` was missing. Add
`--config` with the folder's build type:

```powershell
cmake --build build --config Release --parallel
```

#### CMake can't find Qt 6

CMake stops with `Could not find a package configuration file provided by "Qt6"` when the folder was
configured without vcpkg, for example from the MSYS2 shell. Start it over with `--fresh`:

```powershell
cmake -B build --fresh
```

## MSYS2 UCRT64

### Requirements

Install [MSYS2](https://www.msys2.org/). Then, in the MSYS2 **UCRT64** shell, install the
compiler, CMake and Qt:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-multimedia
```

Use `mingw-w64-ucrt-x86_64-cmake`, not the plain `cmake` package: that one is built for the MSYS
environment and fails to configure Qt.

Build from the UCRT64 shell. gbemu doesn't build from MSYS2's other shells (MSYS, MINGW64,
CLANG64).

### Release build

Every command in this section configures and builds the `build` folder, with the options for that
build on `cmake -B build`. A build folder keeps the options it was configured with, so to change
them on a folder you've already configured, add `--fresh` to `cmake -B build`:

```sh
cmake -B build --fresh -DCMAKE_BUILD_TYPE=Debug
```

To keep several builds side by side, use another folder name in both commands instead of `build`,
such as `build-debug`.

Release is the default. From the repository root:

```sh
cmake -B build
cmake --build build --parallel
```

The executable is created at `build/gbemu.exe`.

### Debug build

Configure the `build` folder with `-DCMAKE_BUILD_TYPE=Debug`, then build it:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

The executable is created at `build/gbemu.exe`.

### Other build types

CMake's other two build types work the same way, with `-DCMAKE_BUILD_TYPE`. RelWithDebInfo is
optimized and has debug information, and MinSizeRel is optimized for a smaller executable:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

### Build with Clang

To build with Clang instead of GCC, install MSYS2's UCRT64 Clang:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-clang
```

Then choose it with `-DCMAKE_CXX_COMPILER`:

```sh
cmake -B build -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
```

It combines with the other options, for example for a Debug build with Clang:

```sh
cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

### Compiler flags

- Release: `-O2`
- Debug: `-g -Og`, which is debuggable but still fast enough to play
- RelWithDebInfo: `-O2 -g`
- MinSizeRel: `-Os`

Every build folder also contains `compile_commands.json`, which editors such as Visual Studio Code
(with clangd) use for code completion and navigation.

### Run

Run gbemu from the UCRT64 shell, so it can find the Qt and GCC DLLs:

```sh
./build/gbemu.exe
```

To start a game straight away, pass the ROM's path:

```sh
./build/gbemu.exe path/to/game.gbc
```

See [Run](README.md#run) in the README for how to use gbemu.

### Updating and rebuilding

To update to the latest source code, pull the changes and build again. CMake rebuilds only what
changed:

```sh
git pull
cmake --build build --parallel
```

To start a build folder over, for example after changing compilers, delete it and configure it
again:

```sh
rm -rf build
cmake -B build
cmake --build build --parallel
```

To update MSYS2's compiler, CMake and Qt, update MSYS2 itself:

```sh
pacman -Syu
```

### Troubleshooting

#### "On Windows, build gbemu with MSVC or from the MSYS2 UCRT64 shell"

CMake was run from an MSYS2 shell other than UCRT64, or with a compiler other than MSVC or MSYS2's
UCRT64 GCC and Clang. Open the **MSYS2 UCRT64** shell from the Start menu, and start the build
folder over:

```sh
rm -rf build
cmake -B build
```

#### CMake can't find Qt 6

CMake stops with `Could not find a package configuration file provided by "Qt6"`. Make sure the
packages in [Requirements](#requirements-1) are installed, and that CMake is MSYS2's UCRT64 CMake:

```sh
which cmake
```

It should print `/ucrt64/bin/cmake`. If it prints `/usr/bin/cmake`, the plain `cmake` package is
installed: install `mingw-w64-ucrt-x86_64-cmake` and start the build folder over.

#### gbemu doesn't start outside the UCRT64 shell

gbemu built with MSYS2 needs the Qt and GCC DLLs in MSYS2's `ucrt64\bin` folder, which only the
UCRT64 shell puts on the path. Start it from the UCRT64 shell.
