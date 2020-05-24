#ifndef SPHERE_H
#define SPHERE_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "display.h"
#include "vecmat.h"

#define SPHERE_PIXELS_AROUND 200
#define SPHERE_PIXELS_VERTICALLY 80

// This struct is BIG. Always use pointers. Makefile has -Werror=stack-usage=bla
struct Sphere {
	struct Vec3 center;
	SDL_Color image[SPHERE_PIXELS_VERTICALLY][SPHERE_PIXELS_AROUND];
};

// Does the sphere contain the camera? If it does, don't try to draw the sphere.
bool sphere_caminside(const struct Sphere *sph);

/*
A part of the sphere is visible to the camera at (0,0,0). The rest isn't. This
plane divides the sphere into the visible part and the part behind the visible
part. The normal vector of the plane points toward the visible side, so
plane_whichside() returns whether a point on the sphere is visible.

Don't call this function if the camera is inside the sphere.
*/
struct Plane sphere_visplane(const struct Sphere *sph);

// Load a sphere from an image file. Free it with malloc when done.
struct Sphere *sphere_load(const char *filename, struct Vec3 center);

// draw sphere to screen if camera is not inside sphere
void sphere_display(const struct Sphere *sph, struct SDL_Renderer *rnd);


#endif  // SPHERE_H
