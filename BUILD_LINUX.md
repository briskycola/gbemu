# Building gbemu on Linux

This guide covers building gbemu from source on the major Linux distributions, with GCC or Clang,
CMake and the Qt 6 packages your distribution provides.

- [Supported distributions](#supported-distributions)
- [Requirements](#requirements)
- [1. Install the dependencies](#1-install-the-dependencies)
- [2. Get the source code](#2-get-the-source-code)
- [3. Build](#3-build)
- [4. Run](#4-run)
- [Build options](#build-options)
- [Updating and rebuilding](#updating-and-rebuilding)
- [Troubleshooting](#troubleshooting)

## Supported distributions

gbemu builds with the packages from these distributions and the distributions based on them:

- **Ubuntu 22.04 or newer**, including Linux Mint 21 or newer, Pop!_OS 22.04 or newer, elementary OS
  7 or newer, and Zorin OS 17 or newer
- **Debian 12 (bookworm) or newer**
- **Fedora and Red Hat Enterprise Linux 9 or newer**, including Rocky Linux, AlmaLinux and CentOS
  Stream 9 or newer
- **Arch Linux**, including Manjaro and EndeavourOS
- **openSUSE Tumbleweed and Leap 16.0 or newer**
- **Gentoo**
- **Void Linux**
- **Alpine Linux**

Other distributions work too, as long as they provide the [requirements](#requirements).

## Requirements

- A C++17 compiler: GCC 9 or newer, or Clang 9 or newer
- CMake 3.22 or newer
- Qt 6.2 or newer, with the Qt Widgets and Qt Multimedia modules and their development files
- Git, to get the source code

## 1. Install the dependencies

Install the compiler, CMake, Git and Qt 6 with your distribution's package manager.

### Ubuntu, Debian, Linux Mint, Pop!_OS

```sh
sudo apt update
sudo apt install build-essential cmake git qt6-base-dev qt6-multimedia-dev libgl-dev
```

`build-essential` provides GCC and `make`. `libgl-dev` provides the OpenGL headers that Qt's CMake
files look for.

### Fedora, RHEL, Rocky Linux, AlmaLinux, CentOS Stream

Fedora, and version 10 or newer of RHEL and its rebuilds, provide Qt 6 in their own repositories:

```sh
sudo dnf install gcc-c++ cmake git qt6-qtbase-devel qt6-qtmultimedia-devel
```

Version 9 gets Qt 6 from [EPEL](https://docs.fedoraproject.org/en-US/epel/) (Extra Packages for
Enterprise Linux), which also needs the CRB repository. Enable both, then install the packages with
the same `dnf install` command as above.

On Rocky Linux 9, AlmaLinux 9 and CentOS Stream 9:

```sh
sudo dnf config-manager --set-enabled crb
sudo dnf install epel-release
```

On RHEL 9:

```sh
sudo subscription-manager repos --enable codeready-builder-for-rhel-9-$(arch)-rpms
sudo dnf install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm
```

### Arch Linux, Manjaro, EndeavourOS

```sh
sudo pacman -S --needed base-devel cmake git qt6-base qt6-multimedia qt6-multimedia-ffmpeg
```

Arch packages include their development files, so there are no separate `-dev` packages.
`qt6-multimedia-ffmpeg` is the backend Qt Multimedia uses for audio. If `pacman` asks which backend
to install, choose `qt6-multimedia-ffmpeg`.

### openSUSE Tumbleweed and Leap

```sh
sudo zypper install gcc-c++ cmake git qt6-widgets-devel qt6-multimedia-devel
```

openSUSE splits Qt's development files by module, so `qt6-widgets-devel` brings in Qt Core, Qt Gui
and Qt Widgets.

### Gentoo

Qt Multimedia plays audio only with one of its audio USE flags, which are off by default. Enable
`pulseaudio`, which works with PulseAudio and with PipeWire's PulseAudio server, the usual setup:

```sh
echo "dev-qt/qtmultimedia pulseaudio" | sudo tee /etc/portage/package.use/gbemu
```

To use PipeWire directly instead, write `pipewire` in place of `pulseaudio`. Then install the
packages:

```sh
sudo emerge --ask dev-build/cmake dev-vcs/git dev-qt/qtbase dev-qt/qtmultimedia
```

Gentoo installs GCC with the base system. Qt Base's default USE flags already include `gui`,
`widgets` and `X`, which gbemu needs.

### Void Linux

```sh
sudo xbps-install -S base-devel cmake git qt6-base-devel qt6-multimedia-devel
```

### Alpine Linux

The Qt 6 packages are in Alpine's `community` repository. If it isn't enabled yet, uncomment its
line in `/etc/apk/repositories`. Then install the packages:

```sh
sudo apk add build-base cmake git qt6-qtbase-dev qt6-qtbase-x11 qt6-qtwayland qt6-qtmultimedia-dev qt6-qtmultimedia-ffmpeg
```

Alpine splits Qt's display support out of Qt Base: `qt6-qtbase-x11` lets gbemu open windows on
X11, and `qt6-qtwayland` on Wayland. `qt6-qtmultimedia-ffmpeg` is the backend Qt Multimedia uses
for audio. On a system without `sudo`, run the command as root with `doas` or `su`.

## 2. Get the source code

Clone the repository and enter it:

```sh
git clone https://github.com/briskycola/gbemu.git
cd gbemu
```

The commands in the rest of this guide run from this folder, the repository root.

## 3. Build

gbemu builds in Release, which is optimized for playing games, or Debug, which is for debugging the
emulator. Release is the default.

Every command in this guide configures and builds the `build` folder, with the options for that
build on `cmake -B build`. A build folder keeps the options it was configured with, so to change
them on a folder you've already configured, add `--fresh` to `cmake -B build`:

```sh
cmake -B build --fresh -DCMAKE_BUILD_TYPE=Debug
```

To keep several builds side by side, use another folder name in both commands instead of `build`,
such as `build-debug`.

### Release build

Configure the `build` folder, then build it:

```sh
cmake -B build
cmake --build build --parallel
```

The executable is created at `build/gbemu`. Release builds are compiled with `-O2`.

### Debug build

Configure the `build` folder with `-DCMAKE_BUILD_TYPE=Debug`, then build it:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

The executable is created at `build/gbemu`. Debug builds are compiled with `-g -Og`, which keeps
them debuggable while still fast enough to play.

### Other build types

CMake's other two build types work the same way, with `-DCMAKE_BUILD_TYPE`:

- **RelWithDebInfo** is optimized like Release, and also has debug information, for debugging at
  full speed. It is compiled with `-O2 -g`.
- **MinSizeRel** is optimized for a smaller executable. It is compiled with `-Os`.

For example, for RelWithDebInfo:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

The executable is created at `build/gbemu`.

## 4. Run

Start gbemu from the repository root:

```sh
./build/gbemu
```

To start a game straight away, pass the ROM's path:

```sh
./build/gbemu path/to/game.gbc
```

gbemu runs from its build folder and doesn't need to be installed. See [Run](README.md#run) in the
README for how to open games, set up the BIOS files, and change the settings.

## Build options

These options go on `cmake -B build`, and can be combined with each other and with
`-DCMAKE_BUILD_TYPE`. For example, a Debug build with Ninja and Clang:

```sh
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

To add an option to a folder you've already configured, add `--fresh` too.

### Use Ninja

[Ninja](https://ninja-build.org/) builds faster than Make. Install it, then configure with
`-G Ninja`:

```sh
cmake -B build -G Ninja
cmake --build build
```

Ninja builds in parallel by default. Its package is `ninja-build` on Ubuntu, Debian, Fedora, RHEL
and Alpine, `dev-build/ninja` on Gentoo, and `ninja` on Arch, openSUSE and Void.

### Use Clang

To build with Clang instead of GCC, install it, then choose it with `-DCMAKE_CXX_COMPILER`:

```sh
cmake -B build -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
```

Its package is `clang` on Ubuntu, Debian, Fedora, RHEL, Arch, openSUSE and Void, and
`llvm-core/clang` on Gentoo. Alpine names its Clang packages by version, such as `clang22`.

### Use a Qt from the Qt installer

gbemu uses your distribution's Qt by default. To use a Qt installed with the
[Qt Online Installer](https://www.qt.io/download-qt-installer) instead, point CMake to it with
`CMAKE_PREFIX_PATH`. For example, for Qt 6.8.0 installed in `~/Qt`:

```sh
cmake -B build -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.0/gcc_64"
cmake --build build --parallel
```

### Editor support

Every build folder contains `compile_commands.json`, which lists how each source file is compiled.
Editors and tools such as clangd, Visual Studio Code, Qt Creator, CLion and Neovim use it for code
completion and navigation. Point your editor to the build folder, or link the file into the
repository root:

```sh
ln -s build/compile_commands.json .
```

Qt Creator and CLion can also open `CMakeLists.txt` directly as a project, and configure and build
gbemu themselves.

## Updating and rebuilding

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

## Troubleshooting

### CMake can't find Qt 6

CMake stops with `Could not find a package configuration file provided by "Qt6"`. Install the Qt
development packages for your distribution, listed in
[Install the dependencies](#1-install-the-dependencies). If Qt is installed somewhere CMake doesn't
look, such as from the Qt Online Installer, pass its location with `CMAKE_PREFIX_PATH`, as in
[Use a Qt from the Qt installer](#use-a-qt-from-the-qt-installer).

### CMake can't find OpenGL

CMake stops with `Could NOT find OpenGL` or `Could NOT find WrapOpenGL` while looking for Qt. Install
the OpenGL development files:

- Ubuntu and Debian: `sudo apt install libgl-dev`
- Fedora and RHEL: `sudo dnf install mesa-libGL-devel`
- openSUSE: `sudo zypper install Mesa-libGL-devel`
- Void: `sudo xbps-install MesaLib-devel`
- Alpine: `sudo apk add mesa-dev`

### CMake is too old

CMake stops with `CMake 3.22 or higher is required`. Every distribution listed in
[Supported distributions](#supported-distributions) provides CMake 3.22 or newer. On an older
release, install a current CMake from [cmake.org](https://cmake.org/download/) or with
`pip install cmake`.

### gbemu doesn't start: "Could not load the Qt platform plugin xcb"

Qt 6.5 and newer need the XCB cursor library to open windows on X11. Install it:

- Ubuntu and Debian: `sudo apt install libxcb-cursor0`
- Fedora and RHEL: `sudo dnf install xcb-util-cursor`
- Arch: `sudo pacman -S xcb-util-cursor`
- openSUSE: `sudo zypper install libxcb-cursor0`
- Gentoo: `sudo emerge --ask x11-libs/xcb-util-cursor`
- Void: `sudo xbps-install xcb-util-cursor`
- Alpine: `sudo apk add xcb-util-cursor`, and make sure `qt6-qtbase-x11` is installed

### There is no sound

Qt Multimedia plays audio through PipeWire or PulseAudio, so make sure one of them is running. Then
check the setup for your distribution:

- Arch and Alpine: make sure `qt6-multimedia-ffmpeg` (Arch) or `qt6-qtmultimedia-ffmpeg` (Alpine)
  is installed.
- Gentoo: make sure `dev-qt/qtmultimedia` is built with the `pulseaudio` or `pipewire` USE flag, as
  in [Gentoo](#gentoo). After changing it, rebuild it with
  `sudo emerge --ask --changed-use dev-qt/qtmultimedia`.

Finally, check that gbemu isn't muted and uses the right output device, on the **Audio** page of
**Settings > Settings...**.
