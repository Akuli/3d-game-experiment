This is a dual-player 3D game with no libraries used for drawing the 3D stuff.


## Setup

Commands to compile and run:

```
$ git submodule init
$ git submodule update
$ sudo apt install gcc make libsdl2-dev libsdl2-mixer-dev libsdl2-ttf-dev
$ make -j2
$ ./game
```

## Broken Things

- With low enough fps and high enough player speed, it's possible to run
  through a wall. This happens when the player can get to the other side
  of the wall before the each-frame-running collision check. Current
  workaround is to not make the players go so fast that this would
  happen at 30fps. (Yes, I know that 30fps is really slow for any gamer)
- Guard-player and guard-enemy collision checking detects a collision
  slightly before the collision actually happens. This is because guard
  balls also have a lower half, and that's just hidden when drawing the
  guards.


## Feature Ideas

- stereo sound: if left player jumps then jump sound (mostly?) to left speaker


## Windows Build

You can build a Windows `.exe` file of this game on Linux.

Start by downloading the Windows "Development Libraries" of SDL2 and SDL_mixer.
Choose MinGW when you need to choose between MinGW and Visual C++.

https://www.libsdl.org/download-2.0.php

https://www.libsdl.org/projects/SDL_mixer/

https://www.libsdl.org/projects/SDL_ttf/

Copy them to a directory named `libs` and extract:

```
$ cd path-to-directory-containing-this-README-file
$ mkdir libs
$ cp ~/Downloads/{SDL2,SDL2_mixer,SDL2_ttf}-devel-*-mingw.tar.gz libs
$ cd libs
$ tar xf SDL2-*
$ tar xf SDL2_mixer-*
$ tar xf SDL2_ttf-*
$ cd ..
```

You may need to replace `~/Downloads/...` with something else if your system isn't
in English. For example, I need `~/Lataukset/...` on my Finnish system.

Do this if you want to use wine for running tests and the produced executable:

```
$ sudo apt install mingw-w64 winehq-stable
$ source winbuildenv
$ make clean
$ make -j2
$ wine build/game.exe
```

Do this if you don't want to use wine:

```
$ sudo apt install mingw-w64
$ source winbuildenv
$ make clean
$ make -j2 build/game.exe
```
