#include "guard.h"
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "ellipsoid.h"
#include "mathstuff.h"
#include "max.h"
#include "log.h"

#define HEIGHT_BASIC 1.0f
#define SPACING_BASIC 0.2f

static struct EllipsoidPic guard_ellipsoidpic;

void guard_init_epic(const SDL_PixelFormat *fmt)
{
	static bool ready = false;
	SDL_assert(!ready);
	ready = true;
	ellipsoidpic_load(&guard_ellipsoidpic, "assets/guard.png", fmt);
}

// this function could be slow with many nonpicked guards
static bool nonpicked_guard_center_in_use(Vec3 center, const struct Ellipsoid *others, int nothers)
{
	for (int i = 0; i < nothers; i++) {
		if (vec3_lengthSQUARED(vec3_sub(center, others[i].botcenter)) < 0.001)
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
			.botcenter = center,
			.epic = &guard_ellipsoidpic,
			.angle = 0,
			.botradius = GUARD_BOTRADIUS,
			.height = HEIGHT_BASIC,
		};
		ellipsoid_update_transforms(&el);
		guards[(*nguards)++] = el;
	}
	SDL_assert(*nguards <= MAX_UNPICKED_GUARDS);
	return howmany2add;
}

int guard_create_unpickeds_random(
	struct Ellipsoid *guards, int *nguards, int howmany2add, const struct Map *map)
{
	Vec3 center = { (rand() % map->xsize) + 0.5f, 0, (rand() % map->zsize) + 0.5f };
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
	float ratio = plr->ellipsoid.height / PLAYER_HEIGHT_NOFLAT;
	float height = ratio*HEIGHT_BASIC;
	float spacing = ratio*SPACING_BASIC;

	arr[0] = (struct Ellipsoid){
		.botcenter = vec3_add(plr->ellipsoid.botcenter, (Vec3){ 0, plr->ellipsoid.height - height/5, 0 }),
		.epic = &guard_ellipsoidpic,
		.angle = plr->ellipsoid.angle,
		.botradius = GUARD_BOTRADIUS,
		.height = height,
	};
	ellipsoid_update_transforms(&arr[0]);

	int n = min(plr->nguards, MAX_PICKED_GUARDS_TO_DISPLAY_PER_PLAYER);
	SDL_assert(n >= 0);
	for (int i = 1; i < n; i++) {
		arr[i] = arr[i-1];
		arr[i].botcenter.y += spacing;
		// no need to update transforms, they don't depend on center location at all
	}
	return n;
}
