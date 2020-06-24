#include "guard.h"
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "ellipsoid.h"
#include "ellipsoidpic.h"
#include "mathstuff.h"
#include "max.h"

#define XZRADIUS 0.25f
#define YRADIUS_BASIC 1.0f
#define SPACING_BASIC 0.15f

static struct EllipsoidPic *get_ellipsoid_pic(const SDL_PixelFormat *fmt)
{
	static struct EllipsoidPic epic;
	static bool ready = false;

	if (!ready) {
		ellipsoidpic_load(&epic, "guard.png", fmt);
		epic.hidelowerhalf = true;
		ready = true;
	}

	SDL_assert(epic.pixfmt == fmt);
	return &epic;
}

// this function could be slow wiht many nonpicked guards
static bool nonpicked_guard_center_in_use(Vec3 center, const struct Ellipsoid *others, int nothers)
{
	for (int i = 0; i < nothers; i++) {
		/*
		It's fine to compare floats with '==' here because:
		- x and z coords are integer+0.5f
		- y coord is SPACING_BASIC + SPACING_BASIC + ... + SPACING_BASIC, some number of times
		None of these float calculations can give inconsistent results.
		*/
		Vec3 oc = others[i].center;
		if (center.x == oc.x && center.y == oc.y && center.z == oc.z)
			return true;
	}
	return false;
}

void guard_create_unpickeds(
	const struct Place *pl, const SDL_PixelFormat *fmt,
	struct Ellipsoid *guards, int *nguards,
	int howmany2add)
{
	int canadd = MAX_UNPICKED_GUARDS - *nguards;
	if (howmany2add > canadd) {
		log_printf("hitting max number of unpicked guards");
		howmany2add = canadd;
	}
	SDL_assert(howmany2add >= 0);

	Vec3 center = { (rand() % pl->xsize) + 0.5f, SPACING_BASIC, (rand() % pl->zsize) + 0.5f };

	for (int i = 0; i < howmany2add; i++) {
		while (nonpicked_guard_center_in_use(center, guards, *nguards))
			center.y += SPACING_BASIC;

		struct Ellipsoid el = {
			.center = center,
			.epic = get_ellipsoid_pic(fmt),
			.angle = 0,
			.xzradius = XZRADIUS,
			.yradius = YRADIUS_BASIC,
		};
		ellipsoid_update_transforms(&el);
		guards[(*nguards)++] = el;
	}
	SDL_assert(*nguards <= MAX_UNPICKED_GUARDS);
}

void guard_unpicked_eachframe(struct Ellipsoid *el)
{
	el->angle += 1.0f / CAMERA_FPS;
	ellipsoid_update_transforms(el);
}

int guard_create_picked(struct Ellipsoid *arr, const struct Player *plr)
{
	if (plr->nguards <= 0)
		return 0;

	/*
	make guards flatten and stretch too.
	Usually ratio is 1, but it's between 0 and 1 when flat, and >1 when stretchy.
	*/
	float ratio = plr->ellipsoid.yradius / PLAYER_YRADIUS_NOFLAT;
	float yradius = ratio*YRADIUS_BASIC;
	float spacing = ratio*SPACING_BASIC;

	arr[0] = (struct Ellipsoid){
		.center = {
			plr->ellipsoid.center.x,
			plr->ellipsoid.center.y + plr->ellipsoid.yradius - yradius/5,
			plr->ellipsoid.center.z,
		},
		.epic = get_ellipsoid_pic(plr->ellipsoid.epic->pixfmt),
		.angle = plr->ellipsoid.angle,
		.xzradius = XZRADIUS,
		.yradius = yradius,
	};
	ellipsoid_update_transforms(&arr[0]);

	int n = min(plr->nguards, MAX_PICKED_GUARDS_TO_DISPLAY_PER_PLAYER);
	SDL_assert(n >= 0);
	for (int i = 1; i < n; i++) {
		arr[i] = arr[i-1];
		arr[i].center.y += spacing;
		// no need to update transforms, they don't depend on center location at all
	}
	return n;
}
