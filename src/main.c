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

// includes all the GameObjects that all players should see
struct GameState {
	struct Player players[2];
	struct Wall walls[4];
};

#if 0
struct GameObj {
	enum { BALL, WALL } kind;
	union { struct Ball *ball; const struct Wall *wall; } ptr;
	float sortz;  // z coordinate in camera coordinates, for sorting
};

// returns -1 if a in front of b, +1 if b in front of a
static int compare_gameobj_pointers(const void *aptr, const void *bptr)
{
	float az = ((const struct GameObj *) aptr)->sortz;
	float bz = ((const struct GameObj *) bptr)->sortz;
	return (az > bz) - (az < bz);
}

static void show_everything(const struct GameState *gs, struct Camera *cam)
{
#define ArrayLen(arr) ( sizeof(arr)/sizeof((arr)[0]) )
	struct GameObj all[ArrayLen(gs->players) + ArrayLen(gs->walls)];
	struct GameObj *allballs = &all[0];
	struct GameObj *allwalls = &all[ArrayLen(gs->players)];

	for (unsigned i = 0; i < ArrayLen(gs->players); i++) {
		allballs[i].kind = BALL;
		allballs[i].ptr.ball = gs->players[i].ball;
		allballs[i].sortz = camera_point_world2cam(cam, gs->players[i].ball->center).z;
	}

	for (unsigned i = 0; i < ArrayLen(gs->walls); i++) {
		allwalls[i].kind = WALL;
		allwalls[i].ptr.wall = &gs->walls[i];

		/*
		Use the z coordinate that is most far away from camera. This should
		make sure that it works well enough when comparing walls and balls.
		*/
		Vec3 corners[4];
		wall_getcamcorners(&gs->walls[i], cam, corners);
		allwalls[i].sortz = min4(corners[0].z, corners[1].z, corners[2].z, corners[3].z);
	}

	qsort(all, ArrayLen(all), sizeof(all[0]), compare_gameobj_pointers);

	for (unsigned i = 0; i < ArrayLen(all); i++) {
		switch(all[i].kind) {
		case BALL:
			ball_display(all[i].ptr.ball, cam);
			break;
		case WALL:
			wall_show(all[i].ptr.wall, cam);
			break;
		}
	}
#undef ArrayLen

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
#endif

#define RAYCAST_PIXEL_SIZE 2

static inline void handle_intersection(Vec3 inter, struct Camera *cam, int x, int y, uint32_t col)
{
	if (camera_point_world2cam(cam, inter).z < 0)
		SDL_FillRect(cam->surface, &(SDL_Rect){ x, y, RAYCAST_PIXEL_SIZE, RAYCAST_PIXEL_SIZE }, col);
}

static void show_everything(const struct GameState *gs, struct Camera *cam)
{
	Mat3 cam2world = mat3_inverse(cam->world2cam);
	SDL_Color colobj = { 0xff, 0xff, 0xff, 0xff };
	uint32_t col = convert_color(cam->surface, colobj);

	for (int x = 0; x < cam->surface->w; x += RAYCAST_PIXEL_SIZE) {
		for (int y = 0; y < cam->surface->h; y += RAYCAST_PIXEL_SIZE) {
			// raycasting
			float xzr = camera_screenx_to_xzr(cam, (float)x);
			float yzr = camera_screeny_to_yzr(cam, (float)y);
			struct Line ray = {
				.point = cam->location,
				.dir = mat3_mul_vec3(cam2world, (Vec3){xzr, yzr, 1}),
			};

			Vec3 inter;
			for (unsigned i = 0; i < sizeof(gs->walls)/sizeof(gs->walls[0]); i++)
				if (wall_intersect_line(&gs->walls[i], ray, &inter))
					handle_intersection(inter, cam, x, y, col);
			for (unsigned i = 0; i < sizeof(gs->players)/sizeof(gs->players[0]); i++)
				if (ball_intersect_line(gs->players[i].ball, ray, &inter))
					handle_intersection(inter, cam, x, y, col);
		}
	}
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
