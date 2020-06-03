// draw all objects of the game, in the correct order

#ifndef SHOWALL_H
#define SHOWALL_H

#include <stddef.h>
#include "ellipsoid.h"
#include "place.h"
#include "wall.h"

#define SHOWALL_MAX_BALLS 2
#define SHOWALL_MAX_OBJECTS (SHOWALL_MAX_BALLS + PLACE_MAX_WALLS)

void show_all(
	const struct Wall *walls, size_t nwalls,
	struct Ellipsoid **els, size_t nels,
	struct Camera *cam);


#endif     // SHOWALL_H
