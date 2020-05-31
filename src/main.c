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

#define PLAYER_COUNT 2
#define WALL_COUNT 4
#define GAMEOBJ_COUNT (PLAYER_COUNT + WALL_COUNT)

// includes all the GameObjects that all players should see
struct GameState {
	struct Player players[PLAYER_COUNT];
	struct Wall walls[WALL_COUNT];
};

struct GameObj {
	enum GameObjKind { BALL, WALL } kind;
	union GameObjPtr { struct Ball *ball; const struct Wall *wall; } ptr;

	// all "dependency" GameObjs are drawn to screen first
	struct GameObj *deps[GAMEOBJ_COUNT];
	size_t ndeps;

	float centerz;   // z coordinate of center in camera coordinates
	bool shown;
};

static int compare_gameobj_ptrs_by_centerz(const void *aptr, const void *bptr)
{
	float az = (*(const struct GameObj **)aptr)->centerz;
	float bz = (*(const struct GameObj **)bptr)->centerz;
	return (az > bz) - (az < bz);
}

// Make sure that dep is shown before obj
// TODO: can be slow with many gameobjs, does it help to keep array always sorted and binsearch?
static void add_dependency(struct GameObj *obj, struct GameObj *dep)
{
	for (size_t i = 0; i < obj->ndeps; i++)
		if (obj->deps[i] == dep)
			return;
	obj->deps[obj->ndeps++] = dep;
}

static void figure_out_deps_for_balls_behind_walls(
	struct GameObj *walls, size_t nwalls,
	struct GameObj *balls, size_t nballs,
	struct Camera *cam)
{
	for (size_t w = 0; w < nwalls; w++) {
		bool camside = wall_side(walls[w].ptr.wall, cam->location);

		for (size_t b = 0; b < nballs; b++) {
			if (wall_side(walls[w].ptr.wall, balls[b].ptr.ball->center) == camside)
				add_dependency(&balls[b], &walls[w]);
			else
				add_dependency(&walls[w], &balls[b]);
		}
	}
}

static void show(struct GameObj *gobj, struct Camera *cam, unsigned int depth)
{
	if (gobj->shown)
		return;

	/*
	Without the sanity check, this recurses infinitely when there is a dependency
	cycle. I don't know whether it's possible to create one, and I want to avoid
	random crashes.
	*/
	if (depth <= GAMEOBJ_COUNT) {
		for (size_t i = 0; i < gobj->ndeps; i++)
			show(gobj->deps[i], cam, depth+1);
	} else {
		nonfatal_error("hitting recursion depth limit");
	}

	switch(gobj->kind) {
	case BALL:
		ball_display(gobj->ptr.ball, cam);
		break;
	case WALL:
		wall_show(gobj->ptr.wall, cam);
		break;
	}
	gobj->shown = true;
}

static void add_gameobj_to_array_if_visible(
	struct GameObj *arr, size_t *len,
	const struct Camera *cam,
	Vec3 center, enum GameObjKind kind, union GameObjPtr ptr)
{
	float centerz = camera_point_world2cam(cam, center).z;
	if (centerz < 0) {   // in front of camera
		arr[*len].kind = kind;
		arr[*len].ptr = ptr;
		arr[*len].centerz = centerz;
		(*len)++;
	}
}

static void get_sorted_gameobj_pointers(
	struct GameObj *walls, size_t nwalls,
	struct GameObj *balls, size_t nballs,
	struct GameObj **res)
{
	for (size_t i = 0; i < nwalls; i++)
		res[i] = &walls[i];
	for (size_t i = 0; i < nballs; i++)
		res[nwalls + i] = &balls[i];
	qsort(res, nwalls + nballs, sizeof(res[0]), compare_gameobj_ptrs_by_centerz);
}

static void show_everything(const struct GameState *gs, struct Camera *cam)
{
	struct GameObj walls[WALL_COUNT] = {0}, balls[PLAYER_COUNT] = {0};
	size_t nwalls = 0, nballs = 0;

	for (size_t i = 0; i < WALL_COUNT; i++)
		add_gameobj_to_array_if_visible(
			walls, &nwalls, cam,
			wall_center(&gs->walls[i]), WALL, (union GameObjPtr){ .wall = &gs->walls[i] });

	for (size_t i = 0; i < PLAYER_COUNT; i++)
		add_gameobj_to_array_if_visible(
			balls, &nballs, cam,
			gs->players[i].ball->center, BALL, (union GameObjPtr){ .ball = gs->players[i].ball });

	struct GameObj *sorted[GAMEOBJ_COUNT];
	figure_out_deps_for_balls_behind_walls(walls, nwalls, balls, nballs, cam);
	get_sorted_gameobj_pointers(walls, nwalls, balls, nballs, sorted);

	for (size_t i = 0; i < nwalls + nballs; i++)
		show(sorted[i], cam, 0);
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

int main(int argc, char **argv)
{
	// TODO: avoid errors caused by not doing sound_init()
	if (!( argc == 2 && strcmp(argv[1], "--no-sound") == 0 ))
		sound_init();

	SDL_Window *win = SDL_CreateWindow(
		"TODO: title here", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, 0);
	if (!win)
		fatal_sdl_error("SDL_CreateWindow failed");

	SDL_Surface *winsurf = SDL_GetWindowSurface(win);
	if (!winsurf)
		fatal_sdl_error("SDL_GetWindowSurface failed");

	struct GameState *gs = calloc(1, sizeof(*gs));
	if (!gs)
		fatal_error("not enough memory");

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

		SDL_FillRect(winsurf, NULL, 0);
		show_everything(gs, &gs->players[0].cam);
		show_everything(gs, &gs->players[1].cam);
		SDL_FillRect(winsurf, &(SDL_Rect){ winsurf->w/2, 0, 1, winsurf->h }, SDL_MapRGB(winsurf->format, 0xff, 0xff, 0xff));
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
