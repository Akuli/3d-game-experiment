## Setup

Take a picture of someone's face and crop it so that you're left with a
rectangle-shaped picture containing the face. Set everything except the
face to transparent. Put that to `person1.png`. Create `person2.png`
similarly. Then:

```
$ git submodule init
$ git submodule update
$ sudo apt install gcc make libsdl2-dev libsdl2-gfx-dev
$ make -j2
$ ./game
```
