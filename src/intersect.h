#ifndef INTERSECT_H
#define INTERSECT_H

#include <stdbool.h>
#include "ellipsoid.h"
#include "wall.h"

// Check for intersections
bool intersect_check_el_el(const struct Ellipsoid *el1, const struct Ellipsoid *el2);

// If intersects, move ellipsoid so that no longer intersects and return true
bool intersect_move_el_el(struct Ellipsoid *el, struct Ellipsoid *other);
bool intersect_move_el_wall(struct Ellipsoid *el, const struct Wall *w);

#endif
