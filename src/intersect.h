#ifndef INTERSECT_H
#define INTERSECT_H

#include <stddef.h>
#include "ellipsoid.h"
#include "mathstuff.h"
#include "wall.h"

enum IntersectElWall {
	INTERSECT_EW_NONE,      // no intersection
	INTERSECT_EW_ELBOTTOM,  // bottom of ellipsoid touches wall
	INTERSECT_EW_ELSIDE,    // side of ellipsoid touches wall
};

enum IntersectElEl {
	INTERSECT_EE_NONE,
	INTERSECT_EE_BOTTOM1_TIP2,  // el1 and el2 stacked, bottom of el1 touches tip of el2
	INTERSECT_EE_BOTTOM2_TIP1,
	INTERSECT_EE_BOTTOM1_SIDE2,
	INTERSECT_EE_BOTTOM2_SIDE1,
};

// Check for intersections
// If mv is not NULL, it is set to how much need move el1 to not intersect (opposite for el2)
enum IntersectElEl intersect_el_el(const struct Ellipsoid *el1, const struct Ellipsoid *el2, Vec3 *mv);
enum IntersectElWall intersect_el_wall(const struct Ellipsoid *el1, const struct Wall *w, Vec3 *mv);

// convenience lol
#define intersects_el_el(EL1, EL2) (intersect_el_el((EL1), (EL2), NULL) != INTERSECT_EE_NONE)
#define intersects_el_wall(EL, WALL) (intersect_el_wall((EL), (WALL), NULL) != INTERSECT_EW_NONE)

#endif
