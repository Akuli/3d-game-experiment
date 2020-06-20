#include "guard.h"
#include <assert.h>
#include <stdbool.h>
#include "ellipsoid.h"
#include "ellipsoidpic.h"
#include "mathstuff.h"

#define XZRADIUS 0.3f

static struct EllipsoidPic *get_ellipsoid_pic(const SDL_PixelFormat *fmt)
{
	static struct EllipsoidPic epic;
	static bool ready = false;

	if (!ready) {
		ellipsoidpic_load(&epic, "guard.png", fmt);
		epic.hidelowerhalf = true;
		ready = true;
	}

	assert(epic.pixfmt == fmt);
	return &epic;
}

struct Ellipsoid guard_create_nonpicked(const struct Place *pl, const SDL_PixelFormat *fmt)
{
	struct Ellipsoid res = {
		.center = {
			(rand() % pl->xsize) + 0.5f,
			0.3f,
			(rand() % pl->zsize) + 0.5f,
		},
		.epic = get_ellipsoid_pic(fmt),
		.angle = 0,
		.xzradius = XZRADIUS,
		.yradius = XZRADIUS,
	};
	ellipsoid_update_transforms(&res);
	return res;
}

void guard_nonpicked_eachframe(struct Ellipsoid *el)
{
	el->angle += 0.01f;
	ellipsoid_update_transforms(el);
}

int guard_create_picked(struct Ellipsoid *arr, const struct Player *plr)
{
	if (plr->nguards <= 0)
		return 0;

	/*
	make hats flatten and stretch too.

	Usually ratio is 1, but it's between 0 and 1 when flat, and >1 when stretchy.
	*/
	float ratio = plr->ellipsoid.yradius / PLAYER_YRADIUS_NOFLAT;
	float yradius = ratio*XZRADIUS;

	arr[0] = (struct Ellipsoid){
		.center = {
			plr->ellipsoid.center.x,
			plr->ellipsoid.center.y + plr->ellipsoid.yradius - yradius/2,
			plr->ellipsoid.center.z,
		},
		.epic = get_ellipsoid_pic(plr->ellipsoid.epic->pixfmt),
		.angle = plr->ellipsoid.angle,
		.xzradius = XZRADIUS,
		.yradius = yradius,
	};
	ellipsoid_update_transforms(&arr[0]);

	for (int i = 1; i < plr->nguards; i++) {
		arr[i] = arr[0];
		arr[i].center.y += yradius/2 * i;
		// no need to update transforms, they don't depend on center location at all
	}
	return plr->nguards;
}
