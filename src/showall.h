// draw all objects of the game, in the correct order

#ifndef SHOWALL_H
#define SHOWALL_H

#include "camera.h"
#include "ellipsoid.h"
#include "rect3.h"

void show_all(
	const struct Rect3 *rects, int nrects,
	const struct Ellipsoid *els, int nels,
	const struct Camera *cam
);


#endif     // SHOWALL_H
