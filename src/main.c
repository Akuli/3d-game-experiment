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

// includes all the objects that all players should see
struct GameState {
	struct Player players[2];
	struct Wall walls[4];
};

static int compare_float_pointers(const void *a, const void *b)
{
	float x = *(const float *)a;
	float y = *(const float *)b;
	return (x>y) - (x<y);
}

static void show_everything(const struct GameState *gs, struct Camera *cam)
{
	for (float x = -10; x <= 10; x += 1.f) {
		for (float z = -10; z <= 0; z += 0.3f) {
			Vec3 worldvec = camera_point_world2cam(cam, (Vec3){ x, 0, z });
			if (worldvec.z < 0) {
				SDL_Point p = camera_point_to_sdl(cam, worldvec);
				SDL_FillRect(
					cam->surface,
					&(SDL_Rect){ p.x, p.y, 1, 1 },
					SDL_MapRGBA(cam->surface->format, 0xff, 0xff, 0x00, 0xff));
			}
		}
	}

	for (unsigned i = 0; i < sizeof(gs->walls)/sizeof(gs->walls[0]); i++)
		wall_show(&gs->walls[i], cam);

	struct PlayerInfo {
		float camcenterz; // z coordinate of center in camera coordinates
		const struct Player *player;
	} arr[] = {
		{ camera_point_world2cam(cam, gs->players[0].ball->center).z, &gs->players[0] },
		{ camera_point_world2cam(cam, gs->players[1].ball->center).z, &gs->players[1] },
	};

	static_assert(offsetof(struct PlayerInfo, camcenterz) == 0, "weird padding");
	qsort(arr, sizeof(arr)/sizeof(arr[0]), sizeof(arr[0]), compare_float_pointers);

	ball_display(arr[0].player->ball, cam);
	ball_display(arr[1].player->ball, cam);
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
Create a surface that manipulates the data of another surface. So, drawing to the
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
