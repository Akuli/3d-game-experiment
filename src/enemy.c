#include "enemy.h"
#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "../generated/filelist.h"
#include "ellipsoid.h"

#define IMAGE_FILE_COUNT 1
#define MOVE_UNITS_PER_SECOND 2.5f

static struct EllipsoidPic *get_ellipsoid_pic(const SDL_PixelFormat *fmt)
{
	static struct EllipsoidPic epics[FILELIST_NENEMIES];
	static bool ready = false;

	if (!ready) {
		for (int i = 0; i < FILELIST_NENEMIES; i++) {
			ellipsoidpic_load(&epics[i], filelist_enemies[i], fmt);
			epics[i].hidelowerhalf = true;
		}
		ready = true;
	}

	SDL_assert(epics[0].pixfmt == fmt);
	return &epics[rand() % FILELIST_NENEMIES];
}

void enemy_init(struct Enemy *en, const SDL_PixelFormat *fmt, const struct Place *pl)
{
	en->ellipsoid = (struct Ellipsoid){
		.center = pl->enemyloc,
		.epic = get_ellipsoid_pic(fmt),
		.angle = 0,
		.xzradius = ENEMY_XZRADIUS,
		.yradius = ENEMY_YRADIUS,
	};
	ellipsoid_update_transforms(&en->ellipsoid);

	en->flags &= ~ENEMY_TURNING;
	en->dir = ENEMY_DIR_XPOS;
}

static enum EnemyDir opposite_direction(enum EnemyDir d)
{
	switch(d) {
		case ENEMY_DIR_XPOS: return ENEMY_DIR_XNEG;
		case ENEMY_DIR_XNEG: return ENEMY_DIR_XPOS;
		case ENEMY_DIR_ZPOS: return ENEMY_DIR_ZNEG;
		case ENEMY_DIR_ZNEG: return ENEMY_DIR_ZPOS;
	}
	log_printf_abort("invalid EnemyDir: %d", d);
}

/*
This runs when the enemy is in the middle of a 1x1 square with integer coordinates
for corners, i.e. when center x and z coordinates are of the form someinteger+0.5
*/
static void begin_turning(struct Enemy *en, const struct Place *pl)
{
	SDL_assert(!(en->flags & ENEMY_TURNING));
	en->flags |= ENEMY_TURNING;

	bool cango[] = {
		[ENEMY_DIR_XPOS] = true,
		[ENEMY_DIR_XNEG] = true,
		[ENEMY_DIR_ZPOS] = true,
		[ENEMY_DIR_ZNEG] = true,
	};
	SDL_assert(sizeof(cango)/sizeof(cango)[0] == 4);   // indexes of cango are the valid enum values

	/*
	 ---------> x
	|
	|
	|   (x,z)      (x+1,z)
	|
	|          en
	|
	|  (x,z+1)    (x+1,z+1)
	|
	V
	z
	*/
	int x = (int) floorf(en->ellipsoid.center.x);
	int z = (int) floorf(en->ellipsoid.center.z);

	for (int i = 0; i < pl->nwalls; i++) {
		switch(pl->walls[i].dir) {

		case WALL_DIR_XY:
			if (pl->walls[i].startx == x && pl->walls[i].startz == z)
				cango[ENEMY_DIR_ZNEG] = false;
			if (pl->walls[i].startx == x && pl->walls[i].startz == z+1)
				cango[ENEMY_DIR_ZPOS] = false;
			break;

		case WALL_DIR_ZY:
			if (pl->walls[i].startx == x && pl->walls[i].startz == z)
				cango[ENEMY_DIR_XNEG] = false;
			if (pl->walls[i].startx == x+1 && pl->walls[i].startz == z)
				cango[ENEMY_DIR_XPOS] = false;
			break;

		}
	}

	// avoid turning around, if possible
	bool canturnaround = cango[opposite_direction(en->dir)];
	cango[opposite_direction(en->dir)] = false;
	if (!cango[0] && !cango[1] && !cango[2] && !cango[3]) {
		if (canturnaround)
			en->dir = opposite_direction(en->dir);
		else
			en->flags |= ENEMY_STUCK;
		return;
	}

	// choose random direction, at least one of them is available (lol)
	while (true) {
		enum EnemyDir dir = rand() % 4;
		if (cango[dir]) {
			en->dir = dir;
			return;
		}
	}
}

/*
Returns whether there is there an integer between the given floats. If there is,
it's also set to ptr.
*/
static bool integer_between_floats(float a, float b, int *ptr)
{
	int abovesmol = (int)ceilf(min(a, b));
	int belowbig = (int)floorf(max(a, b));
	SDL_assert(!( abovesmol < belowbig ));   // if this fails, there's more than 1 integer between

	if (abovesmol == belowbig) {
		*ptr = abovesmol;
		return true;
	}
	return false;
}

/*
If pl is NULL, then don't check whether the enemy should turn instead of moving more
*/
static void move_coordinate(float *coord, float delta, struct Enemy *en, const struct Place *pl)
{
	float old = *coord - 0.5f;    // integer coordinate = turning point
	float new = old + delta;

	int turningpoint;
	if (pl != NULL && integer_between_floats(old, new, &turningpoint)) {
		// must move to turning point and then turn
		*coord = (float)turningpoint + 0.5f;
		begin_turning(en, pl);
	} else {
		*coord = new + 0.5f;
	}
}

static void move(struct Enemy *en, const struct Place *pl)
{
	SDL_assert(!(en->flags & ENEMY_STUCK));

	float amount = 2.5f / CAMERA_FPS;
	switch(en->dir) {
		case ENEMY_DIR_XPOS: move_coordinate(&en->ellipsoid.center.x, +amount, en, pl); break;
		case ENEMY_DIR_XNEG: move_coordinate(&en->ellipsoid.center.x, -amount, en, pl); break;
		case ENEMY_DIR_ZPOS: move_coordinate(&en->ellipsoid.center.z, +amount, en, pl); break;
		case ENEMY_DIR_ZNEG: move_coordinate(&en->ellipsoid.center.z, -amount, en, pl); break;
	}
}

// bring the angle between -pi and pi by changing it only modulo 2pi
static float normalize_angle(float angle)
{
	float pi = acosf(-1);
	angle = fmodf(angle, 2*pi);
	if (angle > pi)
		angle -= 2*pi;
	if (angle < -pi)
		angle += 2*pi;

	SDL_assert(-pi <= angle && angle <= pi);
	return angle;
}

// returns whether destination angle was reached
static bool turn(float *angle, float incr, float destangle)
{
	SDL_assert(incr > 0);

	float diff = normalize_angle(destangle - *angle);
	//*angle += diff; return false;
	if (fabsf(diff) < incr) {
		// so close to destangle that incr would go past destangle
		*angle = destangle;
		return true;
	}

	/*
	Think of diff as "destangle - *angle", and think of diff > 0 as
	"*angle < destangle". This gives the correct result, although it's
	not quite correct because angles can be off by multiples of 2pi.
	*/
	if (diff > 0)
		*angle += incr;
	else
		*angle -= incr;

	return false;
}

static float dir_to_angle(enum EnemyDir dir)
{
	int xdiff = (dir == ENEMY_DIR_XPOS) - (dir == ENEMY_DIR_XNEG);
	int zdiff = (dir == ENEMY_DIR_ZPOS) - (dir == ENEMY_DIR_ZNEG);
	float pi = acosf(-1);

	/*
	The atan2 call returns the angle so that 0 means positive x direction. We want
	it to mean negative z direction instead, because that's how ellipsoidpic works;
	it makes player stuff correspondingly easier, because player looks in negative
	z direction in camera coordinates.
	*/
	return atan2f((float)zdiff, (float)xdiff) + pi/2;
}

void enemy_eachframe(struct Enemy *en, const struct Place *pl)
{
	float angleincr = 4.0f / CAMERA_FPS;
	if (en->flags & ENEMY_STUCK) {
		en->ellipsoid.angle += angleincr;
		ellipsoid_update_transforms(&en->ellipsoid);
	} else if (en->flags & ENEMY_TURNING) {
		bool done = turn(&en->ellipsoid.angle, angleincr, dir_to_angle(en->dir));
		ellipsoid_update_transforms(&en->ellipsoid);
		if (done) {
			en->flags &= ~ENEMY_TURNING;
			move(en, NULL);
		}
	} else {
		move(en, pl);
	}
}
