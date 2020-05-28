#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ball.h"
#include "camera.h"
#include "common.h"
#include "mathstuff.h"
#include "player.h"
#include "sound.h"
#include "wall.h"

#include <SDL2/SDL.h>

#define FPS 60

#define ArrayLen(arr) ( sizeof(arr)/sizeof((arr)[0]) )

// includes all the GameObjects that all players should see
struct GameState {
	struct Player players[2];
	struct Wall walls[4];
};

struct GameObj {
	enum { PLAYER, WALL } kind;
	union { const struct Player *player; const struct Wall *wall; } ptr;
	const struct Camera *camera;   // passing data to qsort without global vars
};


// returns -1 if a in front of b, +1 if b in front of a
static int compare_gameobj_pointers_by_centerz(const void *aptr, const void *bptr)
{
	const struct GameObj *a = aptr, *b = bptr;
	const struct Camera *cam = a->camera;

	if (a->kind == b->kind) {
		// initializing to 0 just to avoid compiler warning, lol
		Vec3 acenter = {0}, bcenter = {0};
		switch(a->kind) {
		case PLAYER:
			acenter = a->ptr.player->ball->center;
			bcenter = b->ptr.player->ball->center;
			break;
		case WALL:
			acenter = wall_center(a->ptr.wall);
			bcenter = wall_center(b->ptr.wall);
			break;
		}

		float az = camera_point_world2cam(cam, acenter).z;
		float bz = camera_point_world2cam(cam, bcenter).z;
		return (az > bz) - (az < bz);
	}

	if (a->kind == WALL && b->kind == PLAYER) {
		struct Plane pl = wall_getplane(a->ptr.wall);

		bool playerfront = plane_whichside(pl, b->ptr.player->ball->center);
		if (mat3_mul_vec3(cam->world2cam, pl.normal).z > 0)
			playerfront = !playerfront;
		return playerfront ? 1 : -1;
	}

	assert(a->kind == PLAYER && b->kind == WALL);
	return -compare_gameobj_pointers_by_centerz(bptr, aptr);
}

static void show_everything(const struct GameState *gs, struct Camera *cam)
{
	struct GameObj all[ArrayLen(gs->players) + ArrayLen(gs->walls)];
	struct GameObj *allplayers = &all[0];
	struct GameObj *allwalls = &all[ArrayLen(gs->players)];

	for (unsigned i = 0; i < ArrayLen(all); i++)
		all[i].camera = cam;

	for (unsigned i = 0; i < ArrayLen(gs->players); i++) {
		allplayers[i].kind = PLAYER;
		allplayers[i].ptr.player = &gs->players[i];
	}

	for (unsigned i = 0; i < ArrayLen(gs->walls); i++) {
		allwalls[i].kind = WALL;
		allwalls[i].ptr.wall = &gs->walls[i];
	}

	qsort(all, ArrayLen(all), sizeof(all[0]), compare_gameobj_pointers_by_centerz);
	for (unsigned i = 0; i < ArrayLen(all); i++) {
		switch(all[i].kind) {
		case PLAYER:
			ball_display(all[i].ptr.player->ball, cam);
			break;
		case WALL:
			wall_show(all[i].ptr.wall, cam);
			break;
		}
	}

	/*for (float x = -10; x <= 10; x += 1.f) {
		for (float z = -10; z <= 0; z += 0.3f) {
			Vec3 worldvec = camera_point_world2cam(cam, (Vec3){ x, 0, z });
			if (worldvec.z < 0) {
				struct FPoint p = camera_point_cam2fpoint(cam, worldvec);
				SDL_FillRect(
					cam->surface,
					&(SDL_Rect){ (int)p.x, (int)p.y, 1, 1 },
					SDL_MapRGBA(cam->surface->format, 0xff, 0xff, 0x00, 0xff));
			}
		}
	}*/
}

// returns whether to continue playing
static bool handle_event(SDL_Event event, struct GameState *gs)
{
	bool down = (event.type == SDL_KEYDOWN);

	switch(event.type) {
	case SDL_QUIT:
		return false;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
		switch(event.key.keysym.scancode) {
		case SDL_SCANCODE_ESCAPE: return false;

		case SDL_SCANCODE_A: player_set_turning(&gs->players[0], 1, down); break;
		case SDL_SCANCODE_D: player_set_turning(&gs->players[0], -1, down); break;
		case SDL_SCANCODE_W: player_set_moving(&gs->players[0], down); break;
		case SDL_SCANCODE_S: player_set_flat(&gs->players[0], down); break;

		case SDL_SCANCODE_LEFT: player_set_turning(&gs->players[1], 1, down); break;
		case SDL_SCANCODE_RIGHT: player_set_turning(&gs->players[1], -1, down); break;
		case SDL_SCANCODE_UP: player_set_moving(&gs->players[1], down); break;
		case SDL_SCANCODE_DOWN: player_set_flat(&gs->players[1], down); break;

		default:
			break;
		}
		break;

	default:
		break;
	}

	return true;
}

/*
Create a surface that refers to another another surface. So, drawing to the
returned surface actually draws to the surface given as argument.
*/
static SDL_Surface *create_half_surface(SDL_Surface *surf, int xoffset, int width)
{
	SDL_Surface *res = SDL_CreateRGBSurfaceFrom(
		(uint32_t *)surf->pixels + xoffset,
		width, surf->h,
		32, surf->pitch, 0, 0, 0, 0);
	if (!res)
		fatal_sdl_error("SDL_CreateRGBSurfaceFROM failed");
	return res;
}

int main(void)
{
	sound_init();

	SDL_Window *win = SDL_CreateWindow(
		"TODO: title here", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, 0);
	if (!win)
		fatal_sdl_error("SDL_CreateWindow failed");

	SDL_Surface *winsurf = SDL_GetWindowSurface(win);
	if (!winsurf)
		fatal_sdl_error("SDL_GetWindowSurface failed");

	struct GameState *gs = malloc(sizeof(*gs));
	if (!gs)
		fatal_error("not enough memory");

	memset(gs, 0, sizeof(*gs));

	gs->players[0].ball = ball_load("players/Tux.png", (Vec3){0,0.5f,-2});
	gs->players[1].ball = ball_load("players/Chick.png", (Vec3){2,0.5f,-2});

	// This turned out to be much faster than blitting
	gs->players[0].cam.surface = create_half_surface(winsurf, 0, winsurf->w/2);
	gs->players[1].cam.surface = create_half_surface(winsurf, winsurf->w/2, winsurf->w/2);

	for (unsigned i = 0; i < sizeof(gs->walls)/sizeof(gs->walls[0]); i++) {
		gs->walls[i].dir = WALL_DIR_Z;
		gs->walls[i].startx = 1;
		gs->walls[i].startz = 2 + (int)i;
		wall_initcaches(&gs->walls[i]);
	}

	uint32_t time = 0;
	while(1){
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if (!handle_event(event, gs))
				goto exit;
		}

		player_eachframe(&gs->players[0], FPS, gs->walls, sizeof(gs->walls)/sizeof(gs->walls[0]));
		player_eachframe(&gs->players[1], FPS, gs->walls, sizeof(gs->walls)/sizeof(gs->walls[0]));

		SDL_FillRect(winsurf, NULL, 0x000000UL);
		show_everything(gs, &gs->players[0].cam);
		show_everything(gs, &gs->players[1].cam);
		SDL_UpdateWindowSurface(win);

		uint32_t curtime = SDL_GetTicks();
		fprintf(stderr, "speed percentage thingy = %.1f%%\n",
			(float)(curtime - time) / (1000/FPS) * 100.f);

		time += 1000/FPS;
		if (curtime <= time) {
			SDL_Delay(time - curtime);
		} else {
			// its lagging
			time = curtime;
		}
	}

exit:
	free(gs->players[0].ball);
	free(gs->players[1].ball);
	SDL_FreeSurface(gs->players[0].cam.surface);
	SDL_FreeSurface(gs->players[1].cam.surface);
	free(gs);
	sound_deinit();
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
