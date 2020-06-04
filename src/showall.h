// draw all objects of the game, in the correct order

#ifndef SHOWALL_H
#define SHOWALL_H

#include <stddef.h>
#include "ellipsoid.h"
#include "place.h"
#include "player.h"
#include "wall.h"

#define SHOWALL_MAX_PLAYERS 2
#define SHOWALL_MAX_ENEMIES 500
#define SHOWALL_MAX_ELLIPSOIDS (SHOWALL_MAX_PLAYERS + SHOWALL_MAX_ENEMIES)
#define SHOWALL_MAX_OBJECTS (PLACE_MAX_WALLS + SHOWALL_MAX_ELLIPSOIDS)

void show_all(
	const struct Wall *walls, size_t nwalls,
	const struct Player *plrs, size_t nplrs,
	const struct Camera *cam);


#endif     // SHOWALL_H
