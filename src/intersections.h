#ifndef INTERSECTIONS_H
#define INTERSECTIONS_H

#include "mathstuff.h"

// Check if line segments intersect. If they do, store an intersection point in *res.
// There can be multiple intersections, if lines go in same direction and overlap.
bool intersect_line_segments(Vec2 start1, Vec2 end1, Vec2 start2, Vec2 end2, Vec2 *res);

// Both tetragons (polygon with 4 corners) should be arrays of 4 elements.
// If returns true, sets ipoint to an example intersection point.
// It can be, and in fact always is, on the boundary of at least one of the two tetragons.
// Assumes convexity.
bool intersect_tetragons(const Vec2 *tetra1, const Vec2 *tetra2, Vec2 *ipoint);

#endif  // INTERSECTIONS_H
