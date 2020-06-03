#ifndef ELLIPSEMOVE_H
#define ELLIPSEMOVE_H

/*
How much should ellipses be moved apart in x direction to make them not intersect
each other? Never returns a negative value. Zero means that the ellipses don't
intersect. Both ellipse equations are given as

	((x - center.x)/a)^2 + ((y - center.y)/b)^2 = 1.
*/
float ellipse_move_amount_x(
	float a1, float b1, Vec2 center1,
	float a2, float b2, Vec2 center2);


#endif    // ELLIPSEMOVE_H
