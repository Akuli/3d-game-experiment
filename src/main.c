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

#define FPS 30

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
	float sortz;     // either centerz or z coordinate from ray casting

	bool shown;
};

// return the intersection of gobj and ray closest to camera, 0 for no intersect
static float intersect(const struct GameObj *gobj, struct Line ray, const struct Camera *cam)
{
	switch(gobj->kind) {
		case WALL:
		{
			Vec3 inter;
			if (!wall_intersect_line(gobj->ptr.wall, ray, &inter))
				return 0;
			return camera_point_world2cam(cam, inter).z;
		}

		case BALL:
		{
			Vec3 inter1, inter2;
			if (!ball_intersect_line(gobj->ptr.ball, ray, &inter1, &inter2))
				return 0;
			float z1 = camera_point_world2cam(cam, inter1).z;
			float z2 = camera_point_world2cam(cam, inter2).z;
			return (z1 + z2)/2;
			return min(z1, z2);
		}
	}

	return 0;    // never runs but makes compiler happy
}

static int compare_gameobj_double_pointers(const void *aptr, const void *bptr)
{
	float az = (*(const struct GameObj **)aptr)->sortz;
	float bz = (*(const struct GameObj **)bptr)->sortz;
	return (az > bz) - (az < bz);
}

static void swap(struct GameObj **a, struct GameObj **b)
{
	struct GameObj *tmp = *a;
	*a = *b;
	*b = tmp;
}

// TODO: can be slow with many gameobjs, does it help to keep it always sorted?
static void add_to_pointer_list(struct GameObj **ptrlist, size_t *len, struct GameObj *ptr)
{
	for (size_t i = 0; i < *len; i++)
		if (ptrlist[i] == ptr)
			return;
	ptrlist[(*len)++] = ptr;
}

/*
Most of the time sorting the objects by their centers is enough to get the correct
drawing order, but sometimes it isn't. To catch those corner cases, we think of
rays, which are 3D lines corresponding to each pixel of the screen. We don't
actually need to do this for every pixel because we also sort by center, which
handles most cases.

Comment out the raycasting code to see an example of a case that sorting by center
doesn't handle alone.
*/
#define PIXELS_PER_RAY_X 10
#define PIXELS_PER_RAY_Y 10

static void get_deps_by_raycasting(struct GameObj **ptrs, size_t nptrs, struct Camera *cam)
{
	Mat3 cam2world = mat3_inverse(cam->world2cam);

	for (int x = 0; x < cam->surface->w; x += PIXELS_PER_RAY_X) {
		for (int y = 0; y < cam->surface->h; y += PIXELS_PER_RAY_Y) {
			float xzr = camera_screenx_to_xzr(cam, (float)x);
			float yzr = camera_screeny_to_yzr(cam, (float)y);
			struct Line ray = {
				.point = cam->location,
				.dir = mat3_mul_vec3(cam2world, (Vec3){xzr, yzr, 1}),
			};

			size_t i = 0, n = nptrs;
			while (i < n) {
				float z = intersect(ptrs[i], ray, cam);
				if (z < 0) {
					ptrs[i++]->sortz = z;
				} else {
					// object doesn't hit ray, ignore for rest of iteration
					swap(&ptrs[--n], &ptrs[i]);
				}
			}

			qsort(ptrs, n, sizeof(ptrs[0]), compare_gameobj_double_pointers);
			for (i = 1; i < n; i++)
				add_to_pointer_list(ptrs[i]->deps, &ptrs[i]->ndeps, ptrs[i-1]);
		}
	}
}

static void sort_by_centerz(struct GameObj **ptrs, size_t nptrs)
{
	for (size_t i = 0; i < nptrs; i++)
		ptrs[i]->sortz = ptrs[i]->centerz;
	qsort(ptrs, nptrs, sizeof(ptrs[0]), compare_gameobj_double_pointers);
}

static void show(struct GameObj *gobj, struct Camera *cam, unsigned int depth)
{
	if (gobj->shown)
		return;
	gobj->shown = true;   // setting this early prevents infinite recursion

	if (depth <= GAMEOBJ_COUNT) {   // some sanity because funny corner cases
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

static void show_everything(const struct GameState *gs, struct Camera *cam)
{
	struct GameObj objs[GAMEOBJ_COUNT] = {0};
	size_t n = 0;
	for (size_t i = 0; i < PLAYER_COUNT; i++)
		add_gameobj_to_array_if_visible(
			objs, &n, cam,
			gs->players[i].ball->center, BALL, (union GameObjPtr){ .ball = gs->players[i].ball });
	for (size_t i = 0; i < WALL_COUNT; i++)
		add_gameobj_to_array_if_visible(
			objs, &n, cam,
			wall_center(&gs->walls[i]), WALL, (union GameObjPtr){ .wall = &gs->walls[i] });

	struct GameObj *ptrs[GAMEOBJ_COUNT];
	for (size_t i = 0; i < n; i++)
		ptrs[i] = &objs[i];

	get_deps_by_raycasting(ptrs, n, cam);
	sort_by_centerz(ptrs, n);

	for (size_t i = 0; i < n; i++)
		show(ptrs[i], cam, 0);

	for (int x = 0; x < cam->surface->w; x += PIXELS_PER_RAY_X)
		for (int y = 0; y < cam->surface->h; y += PIXELS_PER_RAY_Y)
			SDL_FillRect(
				cam->surface,
				&(SDL_Rect){ x, y, 1, 1 },
				SDL_MapRGBA(cam->surface->format, 0xff, 0xff, 0x00, 0xff));
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
