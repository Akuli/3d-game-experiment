#ifndef ENEMY_H
#define ENEMY_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "map.h"

#define ENEMY_BOTRADIUS 0.45f
#define ENEMY_HEIGHT  1.2f

enum EnemyDir {
	ENEMY_DIR_XPOS,
	ENEMY_DIR_XNEG,
	ENEMY_DIR_ZPOS,
	ENEMY_DIR_ZNEG,
};

enum EnemyFlags {
	ENEMY_STUCK = 0x01,     // can't move anywhere, so just spin without changing ellipsoid.center
	ENEMY_TURNING = 0x02,   // soon will be looking into enemy->dir direction
};

struct Enemy {
	const struct Map *map;
	struct Ellipsoid ellipsoid;
	enum EnemyFlags flags;
	enum EnemyDir dir;
};

// call enemy_init_epics() once before calling enemy_new() as many times as you like
void enemy_init_epics(const SDL_PixelFormat *fmt);
struct Enemy enemy_new(const struct Map *map, struct MapCoords loc);

const struct EllipsoidPic *enemy_getrandomepic(void);

// runs fps times per second for each enemy
void enemy_eachframe(struct Enemy *en);


#endif   // ENEMY_H
