# Curses-Strike

This is a 2D-shooter with multiplayer, based on NCurses.

Also known as "Bionicle Counter-Strike", but "Curses-Strike" sounds better.

Architecture/compiler | Status of `master`
--------------------- | ------------------
x86_64 gcc | [![Build Status](https://ultibot.ru/services/traviswh/status.svg?branch=master&job=1)](https://travis-ci.com/CursesGames/CursesStrike)
x86_64 clang | [![Build Status](https://ultibot.ru/services/traviswh/status.svg?branch=master&job=2)](https://travis-ci.com/CursesGames/CursesStrike)
armv7l linaro | [![Build Status](https://ultibot.ru/services/traviswh/status.svg?branch=master&job=3)](https://travis-ci.com/CursesGames/CursesStrike)
mips-24kc openwrt | [![Build Status](https://ultibot.ru/services/traviswh/status.svg?branch=master&job=4)](https://travis-ci.com/CursesGames/CursesStrike)

How to build:
- Clone this repo
- `cd` to root directory of this repo
- If you want to cross-compile,	set up your toolchain in Makefile.*arch* 
- `make` to make release, or `CONFIG=debug make` if you want debugging symbols
- Executables are in `bin` subdirectory
- If you want a single target, just point it, for example: `make cs`
- If you want a single target in debug configuration: `CONFIG=debug make cs`

Feel free to send issues and pull requests.

Copyleft (c) Linux Flowers Team, 2018
