#include "player.h"
#include <stddef.h>
#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "guard.h"
#include "log.h"
#include "mathstuff.h"
#include "map.h"
#include "sound.h"
#include "wall.h"

/*
Most keyboards don't allow arbitrarily many keys to be pressed down at the same
time. Something like two flat, moving and turning players would likely cause
issues with this. To avoid that, we limit things that flat players can do:
- Flat players move slower. They do move though, which means that you can go
  under walls and there is actually a reason to be flat, other than to jump.
- Flat players can't turn at all.
*/

#define NORMAL_SPEED 8.f    // units per second
#define FLAT_SPEED (NORMAL_SPEED/4)
#define RADIANS_PER_SECOND 5.0f

#define CAMERA_BEHIND_PLAYER 4.0f
#define CAMERA_HEIGHT 4.0f

#define GRAVITY 66.0f
#define JUMP_INITIAL_Y_SPEED 20.0f

struct EllipsoidPic *const *player_epics = NULL;
int player_nepics = -1;

void player_init_epics(const SDL_PixelFormat *fmt)
{
	SDL_assert(player_epics == NULL);
	player_epics = ellipsoidpic_loadmany(&player_nepics, "assets/players/*.png", fmt);
	SDL_assert(player_epics != NULL);
}

static float get_y_radius(const struct Player *plr)
{
	if (plr->flat)   // if flat and jumping, then do this
		return PLAYER_HEIGHT_FLAT / 2;
	return PLAYER_YRADIUS_NOFLAT + 0.3f*(plr->ellipsoid.center.y - PLAYER_YRADIUS_NOFLAT);
}

static void keep_ellipsoid_inside_map(struct Ellipsoid *el, const struct Map *map)
{
	clamp_float(&el->center.x, el->xzradius, map->xsize - el->xzradius);
	clamp_float(&el->center.z, el->xzradius, map->zsize - el->xzradius);
}

void player_eachframe(struct Player *plr, const struct Map *map)
{
	// Don't turn while flat. See beginning of this file for explanation.
	if (plr->turning != 0 && !plr->flat) {
		plr->ellipsoid.angle += (RADIANS_PER_SECOND / (float)CAMERA_FPS) * (float)plr->turning;
		// ellipsoid_update_transforms() called below
	}

	if (plr->moving) {
		Vec3 v = mat3_mul_vec3(plr->cam.cam2world, (Vec3){ 0, 0, -(plr->flat ? FLAT_SPEED : NORMAL_SPEED) });
		plr->speed.x = v.x;
		plr->speed.z = v.z;
	} else {
		plr->speed.x = 0;
		plr->speed.z = 0;
	}

	vec3_add_inplace(&plr->ellipsoid.center, vec3_mul_float(plr->speed, 1.0f/CAMERA_FPS));

	if (plr->ellipsoid.center.y < plr->ellipsoid.yradius) {
		plr->speed.y = 0;
		plr->ellipsoid.center.y = plr->ellipsoid.yradius;
	}
	plr->speed.y -= GRAVITY/CAMERA_FPS;

	plr->ellipsoid.xzradius = PLAYER_XZRADIUS;
	plr->ellipsoid.yradius = get_y_radius(plr);
	ellipsoid_update_transforms(&plr->ellipsoid);

	for (int i = 0; i < map->nwalls; i++)
		wall_bumps_ellipsoid(&map->walls[i], &plr->ellipsoid);
	keep_ellipsoid_inside_map(&plr->ellipsoid, map);

	Vec3 diff = { 0, 0, CAMERA_BEHIND_PLAYER };
	vec3_apply_matrix(&diff, mat3_rotation_xz(plr->ellipsoid.angle));

	plr->cam.angle = plr->ellipsoid.angle;
	plr->cam.location = vec3_add(plr->ellipsoid.center, diff);
	plr->cam.location.y = CAMERA_HEIGHT;

	camera_update_caches(&plr->cam);
}

void player_set_turning(struct Player *plr, int dir, bool turn)
{
	SDL_assert(dir == 1 || dir == -1);

	if (turn)
		plr->turning = dir;
	else if (plr->turning == dir)
		plr->turning = 0;
}

void player_set_moving(struct Player *plr, bool mv)
{
	plr->moving = mv;
}

void player_set_flat(struct Player *plr, bool flat)
{
	if (plr->flat == flat)
		return;

	plr->flat = flat;

	if (plr->flat)
		sound_play("lemonsqueeze.wav");
	else {
		sound_play("pop.wav");
		if (plr->ellipsoid.center.y <= PLAYER_YRADIUS_NOFLAT) {
			log_printf("jump start");
			sound_play("boing.wav");
			plr->speed.y = JUMP_INITIAL_Y_SPEED;
		} else {
			log_printf("user attempted to jump, but player not low enough");
		}
	}
}

void player_drop_guard(struct Player *plr, struct Ellipsoid *arr, int *arrlen)
{
	if (plr->nguards <= 0)
		return;

	// Adding the little 1e-5f helps to prevent picking up guard immediately
	Vec3 dropdiff = { 0, -plr->ellipsoid.yradius, PLAYER_XZRADIUS + GUARD_XZRADIUS + 1e-5f };
	vec3_apply_matrix(&dropdiff, plr->cam.cam2world);
	Vec3 loc = vec3_add(plr->ellipsoid.center, dropdiff);

	int n = guard_create_unpickeds_center(arr, arrlen, 1, loc);
	plr->nguards -= n;
	if (n != 0)
		sound_play("leave.wav");
}
