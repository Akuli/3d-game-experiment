# 3D game experiment

This is a dual-player 3D game with no libraries used for drawing the 3D stuff.

![screenshot](screenshot.png)


## Setup

If you are on Windows, you can download this game by clicking "Releases" on GitHub.
Unzip it and run `build\game.exe`.

To develop this game, you need some other operating system.

```
$ git clone https://github.com/Akuli/3d-game-experiment
$ cd 3d-game-experiment
$ git submodule init
$ git submodule update
$ sudo apt install gcc make libsdl2-dev libsdl2-mixer-dev libsdl2-ttf-dev
$ make -j2 && ./game
```

I don't know what you should do if you don't have `apt`.

## Key Bindings

In the game:
- Moving around and flattening: right player uses arrow keys, left player uses W, A, S and D imagining that they are arrow keys.
- Dropping guards behind: left player uses F, right player uses zero.
  If there's no zero key next to the arrow keys on your keyboard, then
  please [let me know](https://github.com/Akuli/3d-game-experiment/issues/new)
  which key would be more convenient for you.

In the player and map choosing screen, you can click the buttons or press these keys:
- Player chooser: right player uses left and right arrow keys, left player users A and D.
- Map chooser: up and down arrow keys, or W and S. The purpose is that both players can use the map chooser.
- Play button: enter or space.

In the game over screen, you can again click buttons or press these keys:
- Play again button: F5. This is for compatibility with games where you can play again by refreshing the browser window.
- Player and map chooser button: enter or space.

In the map editor, the keyboard is quite limited (use mouse instead):
- Selection: arrow keys
- Add/remove wall: enter
- Done editing: Escape
- Add enemy: E

Map editor's delete confirming dialog:
- Yes: Y
- No: N or Escape


## Broken Things

- With low enough fps and high enough player speed, it's possible to run
  through a wall. This happens when the player can get to the other side
  of the wall before the each-frame-running collision check. Current
  workaround is to not make the players go so fast that this would
  happen at 30fps. (Yes, I know that 30fps is really slow for any gamer)
- Unpicked guards that are far above ground can be picked up again without
  actually touching them. The reason is that guards are ellipsoids (stretched
  balls) that also have a hidden lower half, but the collision checking isn't
  aware of that lower half. Unpicked guards can be put high up by either dropping
  many guards to exactly the same place or jumping and dropping.


## Feature Ideas

- spinning game over display
- stereo sound: if left player jumps then jump sound (mostly?) to left speaker
- network multiplayer lol?
- A player that looks exactly like an enemy


## Conventions in this project

- To avoid a global variable, it's fine to do this...

    ```c
    struct Foo *get_foos()
    {
        static struct Foo res[N_FOOS];
        static bool ready = false;
        if (ready)
            return res;

        for (int i = 0; i < N_FOOS; i++)
            res[i] = create_foo();
        ready = true;
        return res;
    }
    ```

    ...except when that takes a long time on startup. In that case, I use a
    global variable instead. This isn't as bad as you might think because:
    - As opposed to lazy-loading everything, I can display meaningful
      "Loading bla..." texts while the game starts.
    - I don't need to pass around a lot of variables everywhere.
    - All global variables are intended to be immutable, and there should be no
      mutable global state to cause problems.

- My coding style is linux-kernel-ish, but I'm not at all nit-picky about it.
  Contributions are welcome, although I usually don't get many, especially in
  projects written in C.


## Windows Build

GitHub Actions builds a Windows `.exe` file of this game when pushing to master.
See the config files in `.github/workflows/` and use `make clean` generously
if you need to do it without GitHub Actions.
