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
- `make help` will give you some help
- `CONFIG=debug make` if you want the most debuggable output, OR
- `CONFIG=prerelease make` if you want release with debugging symbols __(this is the default)__, OR
- `CONFIG=release make` to make release, OR
- `CONFIG=wild make` to make wild release with the smallest size
- Executables are in `bin` subdirectory
- If you want a single target, just point it, for example: `make cs`
- If you want a single target in certain configuration: `CONFIG=release make cs`

If you want to cross-compile:
- Set up your toolchain in Makefile.*arch*, or pass `CROSS_COMPILE=<toolchain-canonical-name>` to the environment
- Add `-f Makefile.*arch*` to the every `make` command, for example: `CONFIG=release make -f Makefile.MIPS cs`

Feel free to send issues and pull requests.

Copyleft (c) Linux Flowers Team, 2018

![Linux Flowers](LinuxFlowers.png)
