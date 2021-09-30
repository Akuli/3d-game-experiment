#ifndef INTERSECT_H
#define INTERSECT_H

#include <stdbool.h>
#include "ellipsoid.h"
#include "wall.h"

enum Intersect {
	INTERSECT_NONE,      // no intersection
	INTERSECT_ELBOTTOM,  // bottom of first ellipsoid touches top of wall or second ellipsoid
	INTERSECT_SIDE,      // anything else
};

// Check for intersections
// If mv is not NULL, it is set to how much need move el1 to not intersect
enum Intersect intersect_el_el(const struct Ellipsoid *el1, const struct Ellipsoid *el2, Vec3 *mv);
enum Intersect intersect_el_wall(const struct Ellipsoid *el1, const struct Wall *w, Vec3 *mv);

// convenience lol
#define intersects_el_el(EL1, EL2) (intersect_el_el((EL1), (EL2), NULL) != INTERSECT_NONE)
#define intersects_el_wall(EL, WALL) (intersect_el_wall((EL), (WALL), NULL) != INTERSECT_NONE)

#endif
