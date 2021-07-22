#!/usr/bin/env python3
#
# Usage:
#
#   python3 scripts/generate_random_map.py | tee assets/default_maps/foo.txt
#
# Repeat that until the printed map is good enough, and then modify the
# output file by hand however you want

import enum
import random

WIDTH = 40
HEIGHT = 20
WALL_COUNT = 700


# each wall is represented as (WallDirection, startx, startz)
class WallDirection(enum.Enum):
    XY = 1
    ZY = 2


def show_walls(walls):
    lines = []
    for z in range(HEIGHT + 1):
        line = ''
        for x in range(WIDTH + 1):
            if (WallDirection.XY, x, z) in walls:
                line += ' --'
            else:
                line += '   '
        lines.append(line.rstrip())

        line = ''
        for x in range(WIDTH + 1):
            if (WallDirection.ZY, x, z) in walls:
                line += '|  '
            else:
                line += '   '
        lines.append(line.rstrip())

    while lines and not lines[-1]:
        del lines[-1]
    print('\n'.join(lines))


all_walls = {
    (WallDirection.XY, x, z)
    for x in range(WIDTH)
    for z in range(HEIGHT + 1)
} | {
    (WallDirection.ZY, x, z)
    for x in range(WIDTH + 1)
    for z in range(HEIGHT)
}

boundary_walls = {
    (WallDirection.XY, x, z)
    for x in range(WIDTH)
    for z in [0, HEIGHT]
} | {
    (WallDirection.ZY, x, z)
    for x in [0, WIDTH]
    for z in range(HEIGHT)
}
assert boundary_walls.issubset(all_walls)

how_many_missing = WALL_COUNT - len(boundary_walls)
walls = boundary_walls.copy()
walls.update(random.sample(all_walls - walls, how_many_missing))
show_walls(walls)
