// draw all objects of the game, in the correct order

#ifndef SHOWALL_H
#define SHOWALL_H

#include <stddef.h>
#include "camera.h"
#include "enemy.h"
#include "player.h"
#include "wall.h"

#define SHOWALL_MAX_ELLIPSOIDS 500

// camera surfaces can be smaller than this, but these are handy for loop bounds
#define SHOWALL_SCREEN_WIDTH 800
#define SHOWALL_SCREEN_HEIGHT 600

void show_all(
	const struct Wall *walls, int nwalls,
	const struct Ellipsoid *els, int nels,
	const struct Camera *cam);


#endif     // SHOWALL_H
