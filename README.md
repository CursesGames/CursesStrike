# Curses-Strike
Status of `master` on x86_64: [![Build Status](https://travis-ci.com/CursesGames/CursesStrike.svg?branch=master)](https://travis-ci.com/CursesGames/CursesStrike)

This is a 2D-shooter with multiplayer, based on NCurses.
Also known as "Bionicle Counter-Strike", but "Curses-Strike" sounds better.

How to build:
- Clone this repo
- `cd` to root directory of this repo
- If you want to cross-compile,	set up your toolchain in Makefile.*arch* 
- `make` (is an alias to `make release`) or `make debug` if you want debugging symbols
- Executables are in `bin` subdirectory
- If you want a single target, just point it, for example: `make cs` or `make csds`

Please note that cross-compiling is a beta feature and something may fail! :(
Feel free to send issues and pull requests.

Copyleft (c) cuzwearepanda, 2018
