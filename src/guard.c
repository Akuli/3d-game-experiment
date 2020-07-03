#include "guard.h"
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "ellipsoid.h"
#include "mathstuff.h"
#include "max.h"
#include "log.h"

#define YRADIUS_BASIC 1.0f
#define SPACING_BASIC 0.2f

static struct EllipsoidPic guard_ellipsoidpic;

void guard_init_epic(const SDL_PixelFormat *fmt)
{
	static bool ready = false;
	SDL_assert(!ready);
	ready = true;

	ellipsoidpic_load(&guard_ellipsoidpic, "guard.png", fmt);
	guard_ellipsoidpic.hidelowerhalf = true;
}

// this function could be slow wiht many nonpicked guards
static bool nonpicked_guard_center_in_use(Vec3 center, const struct Ellipsoid *others, int nothers)
{
	for (int i = 0; i < nothers; i++) {
		/*
		It's fine to compare floats with '==' here because:
		- x and z coords are integer+0.5f (or something very unlikely to
		  collide for guards dropped by players)
		- y coord is SPACING_BASIC + SPACING_BASIC + ... + SPACING_BASIC,
		  some number of times
		None of these float calculations can give inconsistent results.
		*/
		Vec3 oc = others[i].center;
		if (center.x == oc.x && center.y == oc.y && center.z == oc.z)
			return true;
	}
	return false;
}

int guard_create_unpickeds_center(
	struct Ellipsoid *guards, int *nguards, int howmany2add, Vec3 center)
{
	int canadd = MAX_UNPICKED_GUARDS - *nguards;
	if (howmany2add > canadd) {
		log_printf("hitting MAX_UNPICKED_GUARDS=%d and adding only %d guards (%d requested)",
			MAX_UNPICKED_GUARDS, canadd, howmany2add);
		howmany2add = canadd;
	}
	SDL_assert(howmany2add >= 0);

	for (int i = 0; i < howmany2add; i++) {
		while (nonpicked_guard_center_in_use(center, guards, *nguards))
			center.y += SPACING_BASIC;

		struct Ellipsoid el = {
			.center = center,
			.epic = &guard_ellipsoidpic,
			.angle = 0,
			.xzradius = GUARD_XZRADIUS,
			.yradius = YRADIUS_BASIC,
		};
		ellipsoid_update_transforms(&el);
		guards[(*nguards)++] = el;
	}
	SDL_assert(*nguards <= MAX_UNPICKED_GUARDS);
	return howmany2add;
}

int guard_create_unpickeds_random(
	struct Ellipsoid *guards, int *nguards, int howmany2add, const struct Place *pl)
{
	Vec3 center = { (rand() % pl->xsize) + 0.5f, 0, (rand() % pl->zsize) + 0.5f };
	return guard_create_unpickeds_center(guards, nguards, howmany2add, center);
}

void guard_unpicked_eachframe(struct Ellipsoid *el)
{
	el->angle += 3.0f / CAMERA_FPS;
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
		.epic = &guard_ellipsoidpic,
		.angle = plr->ellipsoid.angle,
		.xzradius = GUARD_XZRADIUS,
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
