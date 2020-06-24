// draw all objects of the game, in the correct order

#ifndef SHOWALL_H
#define SHOWALL_H

#include "camera.h"
#include "ellipsoid.h"
#include "wall.h"

void show_all(
	const struct Wall *walls, int nwalls,
	const struct Ellipsoid *els, int nels,
	const struct Camera *cam);


#endif     // SHOWALL_H
