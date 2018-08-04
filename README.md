# Curses-Strike

This is a 2D-shooter with multiplayer, based on NCurses.

Also known as "Bionicle Counter-Strike", but "Curses-Strike" sounds better.

Status of `master` on x86_64: 
gcc: ![Build Status](https://ultibot.ru/services/traviswh/status.svg?branch=master&job=1)
clang: ![Build Status](https://ultibot.ru/services/traviswh/status.svg?branch=master&job=2)

Status of `master` on armv7l: ![Build Status](https://ultibot.ru/services/traviswh/status.svg?branch=master&job=3)

Status of `master` on mips-24kc: ![Build Status](https://ultibot.ru/services/traviswh/status.svg?branch=master&job=4)

How to build:
- Clone this repo
- `cd` to root directory of this repo
- If you want to cross-compile,	set up your toolchain in Makefile.*arch* 
- `make` (is an alias to `make release`) or `make debug` if you want debugging symbols
- Executables are in `bin` subdirectory
- If you want a single target, just point it, for example: `make cs` or `make csds`

Feel free to send issues and pull requests.

Copyleft (c) Linux Flowers Team, 2018
