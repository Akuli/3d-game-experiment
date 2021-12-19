#include "jumper.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <stdint.h>
#include "camera.h"
#include "log.h"
#include "rect3.h"
#include "misc.h"

#define MAX_HEIGHT 0.5f
#define RADIUS 0.4f

static struct Rect3Image *jumper_image = NULL;

static void free_image(void)
{
	free(jumper_image);
}

void jumper_init_global_image(const SDL_PixelFormat *pixfmt)
{
	SDL_assert(jumper_image == NULL);
	jumper_image = rect3_load_image("assets/jumper.png", pixfmt);
	atexit(free_image);
}

static struct Rect3 get_rect(const struct Jumper *jmp)
{
	SDL_assert(jumper_image != NULL);
	return (struct Rect3){
		.corners = {
			{ jmp->x + 0.5f - RADIUS, jmp->y, jmp->z + 0.5f - RADIUS },
			{ jmp->x + 0.5f + RADIUS, jmp->y, jmp->z + 0.5f - RADIUS },
			{ jmp->x + 0.5f + RADIUS, jmp->y, jmp->z + 0.5f + RADIUS },
			{ jmp->x + 0.5f - RADIUS, jmp->y, jmp->z + 0.5f + RADIUS },
		},
		.img = jumper_image,
	};
}

struct Rect3 jumper_eachframe(struct Jumper *jmp)
{
	jmp->y += 1.0f / CAMERA_FPS;
	clamp_float(&jmp->y, 0, MAX_HEIGHT);
	return get_rect(jmp);
}

bool jumper_press(struct Jumper *jmp, const struct Ellipsoid *el)
{
	// When looking from the side, jmp is a horizontal line and el is a 2D ellipse.
	float dx = (jmp->x+0.5f) - el->center.x;
	float dz = (jmp->z+0.5f) - el->center.z;

	float h;  // new height of the jumper where it would touch ellipsoid
	if (dx*dx + dz*dz < RADIUS*RADIUS) {
		// directly on top of jumper
		h = el->center.y - el->yradius;
	} else {
		float distbetween = sqrtf(dx*dx + dz*dz);
		/*
		Let's add new x and y coordinates so that the ellipse is (x/a)^2 + (y/b)^2 = 1.
		Then the point (distbetween - RADIUS, h - el->center.y) is on the ellipse.
		*/
		float a = el->xzradius;
		float b = el->yradius;
		float x = distbetween-RADIUS;
		float undersqrt = 1 - (x*x)/(a*a);
		if (undersqrt < 0)
			return false;   // no intersection
		h = el->center.y - b*sqrtf(undersqrt);
	}

	jmp->y = min(jmp->y, h);
	clamp_float(&jmp->y, 0, MAX_HEIGHT);
	return h < MAX_HEIGHT/5;
}
