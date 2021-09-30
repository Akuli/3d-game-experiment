#ifndef INTERSECT_H
#define INTERSECT_H

#include <stdbool.h>
#include "ellipsoid.h"
#include "wall.h"

enum Intersect {
	INTERSECT_NONE,  // no intersection
	INTERSECT_TOP,   // bottom of one ellipsoid touches top of wall or another ellipsoid
	INTERSECT_SIDE,  // anything else
};

// Check for intersections
enum Intersect intersect_check_el_el(const struct Ellipsoid *el1, const struct Ellipsoid *el2);
enum Intersect intersect_check_el_wall(const struct Ellipsoid *el1, const struct Wall *w);

// If intersects, move ellipsoid so that no longer intersects and return true
enum Intersect intersect_move_el_el(struct Ellipsoid *el, struct Ellipsoid *other);
enum Intersect intersect_move_el_wall(struct Ellipsoid *el, const struct Wall *w);

#endif
