// draw all objects of the game, in the correct order

#ifndef SHOWALL_H
#define SHOWALL_H

#include <stddef.h>
#include "ball.h"
#include "wall.h"

#define SHOWALL_MAX_BALLS 2
#define SHOWALL_MAX_WALLS 50
#define SHOWALL_MAX_OBJECTS (SHOWALL_MAX_BALLS + SHOWALL_MAX_WALLS)

void show_all(
	const struct Wall *walls, size_t nwalls,
	struct Ball **balls, size_t nballs,
	struct Camera *cam);


#endif     // SHOWALL_H
