#include "player.h"
#include <math.h>
#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "guard.h"
#include "mathstuff.h"
#include "misc.h"
#include "place.h"
#include "sound.h"
#include "wall.h"
#include "../generated/filelist.h"

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

#define JUMP_MAX_HEIGHT 3.0f
#define JUMP_DURATION_SEC 0.6f

struct EllipsoidPic player_ellipsoidpics[FILELIST_NPLAYERS];

void player_init_epics(const SDL_PixelFormat *fmt)
{
	static bool inited = false;
	SDL_assert(!inited);
	inited = true;

	for (int i = 0; i < FILELIST_NPLAYERS; i++)
		ellipsoidpic_load(&player_ellipsoidpics[i], filelist_players[i], fmt);
}

void player_epic_name(const struct EllipsoidPic *epic, char *name, int sizeofname)
{
	int i = epic - player_ellipsoidpics;
	SDL_assert(0 <= i && i < FILELIST_NPLAYERS);
	misc_basename_without_extension(filelist_players[i], name, sizeofname);
}

static float get_jump_height(int jumpframe)
{
	float time = (float)jumpframe / (float)CAMERA_FPS;

	/*
	Parabola that intersects time axis at time=0 and time=JUMP_DURATION_SEC, having
	max value of JUMP_MAX_HEIGHT
	*/
	float a = (-4*JUMP_MAX_HEIGHT)/(JUMP_DURATION_SEC*JUMP_DURATION_SEC);
	return a*(time - 0)*(time - JUMP_DURATION_SEC);
}

static float get_y_radius(const struct Player *plr)
{
	if (plr->flat)   // if flat and jumping, then do this
		return PLAYER_HEIGHT_FLAT / 2;

	return PLAYER_YRADIUS_NOFLAT + 0.3f*get_jump_height(plr->jumpframe);
}

static void keep_ellipsoid_inside_place(struct Ellipsoid *el, const struct Place *pl)
{
	el->center.x = max(el->xzradius, min(pl->xsize - el->xzradius, el->center.x));
	el->center.z = max(el->xzradius, min(pl->zsize - el->xzradius, el->center.z));
}

void player_eachframe(struct Player *plr, const struct Place *pl)
{
	// Don't turn while flat. See beginning of this file for explanation.
	if (plr->turning != 0 && !plr->flat) {
		plr->ellipsoid.angle += (RADIANS_PER_SECOND / (float)CAMERA_FPS) * (float)plr->turning;
		// ellipsoid_update_transforms() called below
	}

	if (plr->moving) {
		float speed = plr->flat ? FLAT_SPEED : NORMAL_SPEED;
		Vec3 diff = mat3_mul_vec3(plr->cam.cam2world, (Vec3){ 0, 0, -speed/CAMERA_FPS });
		vec3_add_inplace(&plr->ellipsoid.center, diff);
	}

	float y = 0;
	if (plr->jumpframe != 0) {
		plr->jumpframe++;
		y = get_jump_height(plr->jumpframe);
		if (y < 0) {
			// land
			plr->jumpframe = 0;
			y = 0;
		}
	}

	plr->ellipsoid.xzradius = PLAYER_XZRADIUS;
	plr->ellipsoid.yradius = get_y_radius(plr);
	ellipsoid_update_transforms(&plr->ellipsoid);

	plr->ellipsoid.center.y = y + plr->ellipsoid.yradius;

	for (int i = 0; i < pl->nwalls; i++)
		wall_bumps_ellipsoid(&pl->walls[i], &plr->ellipsoid);
	keep_ellipsoid_inside_place(&plr->ellipsoid, pl);

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
		if (plr->jumpframe == 0) {
			sound_play("boing.wav");
			plr->jumpframe = 1;
		}
	}
}

void player_drop_guard(struct Player *plr, struct Ellipsoid *arr, int *arrlen)
{
	if (plr->nguards <= 0)
		return;

	Vec3 movingdir = { cosf(plr->ellipsoid.angle+acosf(-1)/2), 0, sinf(plr->ellipsoid.angle+acosf(-1)/2) };
	float offset = PLAYER_XZRADIUS + GUARD_XZRADIUS + 1e-5f;
	Vec3 loc = vec3_sub(plr->ellipsoid.center, vec3_mul_float(movingdir, -offset));

	int n = guard_create_unpickeds_xz(arr, arrlen, 1, loc.x, loc.z);
	plr->nguards -= n;
	if (n != 0)
		sound_play("leave.wav");
}
