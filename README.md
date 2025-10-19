# Game Boy Emulator
This is a simple game boy emulator written in C
The goal of this project is to gain a better
understanding of computer architecture, emulation,
and systems programming.

## References
I used many resources when building this emulator.
Without these, I would've had to reverse engineer
the game boy, which is not something I can easily do.

1. https://gbdev.io/pandocs/
1. https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html
1. https://www.youtube.com/@lowleveldevel1712

## Building

### Linux
To build on Linux, you will need a C/C++ toolchain
along with SDL2 development libraries. Below are
some commands to install the dependencies on
most Linux distributions:

Arch linux
```bash
sudo pacman -S git base-devel sdl2
```

Debian/Ubuntu/Linux Mint
```bash
sudo apt update
sudo apt install git build-essential libsdl2-dev
```

Fedora
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install git SDL2-devel
```

openSUSE
```bash
sudo zypper install -t pattern devel_basis
sudo zypper install git libSDL2-devel
```

Alpine
```bash
sudo apk add git build-base sdl2-dev
```

Void Linux
```bash
sudo xbps-install git base-devel SDL2-devel
```

To compile the emulator, run the following
commands:
```bash
git clone https://github.com/briskycola/gbemu.git
cd gbemu/src
make -j$(nproc)
```
