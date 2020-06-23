#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "camera.h"
#include "common.h"
#include "display.h"
#include "player.h"
#include "sphere.h"
#include "vecmat.h"

#include <SDL2/SDL.h>

#define FPS 60

// includes all the objects that all players should see
struct GameState {
	struct Player players[2];
};

static void show_everything(const struct GameState *gs, const struct Camera *cam)
{
	for (float x = -10; x <= 10; x += 1.f) {
		for (float z = -10; z <= 0; z += 0.3f) {
			struct Vec3 worldvec = camera_point_world2cam(cam, (struct Vec3){ x, 0, z });
			if (worldvec.z < 0) {
				SDL_Point p = display_point_to_sdl(cam->surface, worldvec);
				SDL_FillRect(
					cam->surface,
					&(SDL_Rect){ p.x, p.y, 1, 1 },
					SDL_MapRGBA(cam->surface->format, 0xff, 0xff, 0x00, 0xff));
			}
		}
	}

	for (int i = 0; i < 2; i++)
		sphere_display(gs->players[i].sphere, cam);
}

// returns whether to continue playing
static bool handle_event(SDL_Event event, struct GameState *gs)
{
	switch(event.type) {
	case SDL_QUIT:
		return false;

	case SDL_KEYDOWN:
		switch(event.key.keysym.scancode) {
		case SDL_SCANCODE_ESCAPE:
			return false;

		case SDL_SCANCODE_A:
			gs->players[0].turning = 1;
			break;
		case SDL_SCANCODE_D:
			gs->players[0].turning = -1;
			break;
		case SDL_SCANCODE_W:
			gs->players[0].moving = true;
			break;

		case SDL_SCANCODE_LEFT:
			gs->players[1].turning = 1;
			break;
		case SDL_SCANCODE_RIGHT:
			gs->players[1].turning = -1;
			break;
		case SDL_SCANCODE_UP:
			gs->players[1].moving = true;
			break;

		default:
			break;
		}
		break;

	case SDL_KEYUP:
		switch(event.key.keysym.scancode) {

		case SDL_SCANCODE_A:
		case SDL_SCANCODE_D:
			gs->players[0].turning = 0;
			break;
		case SDL_SCANCODE_W:
			gs->players[0].moving = false;
			break;

		case SDL_SCANCODE_RIGHT:
		case SDL_SCANCODE_LEFT:
			gs->players[1].turning = 0;
			break;
		case SDL_SCANCODE_UP:
			gs->players[1].moving = false;
			break;

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
	return SDL_CreateRGBSurfaceFrom(
		(uint32_t *)surf->pixels + xoffset,
		width, surf->h,
		32, surf->pitch, 0, 0, 0, 0);
}

int main(void)
{
	SDL_Window *win = SDL_CreateWindow(
		"TODO: title here", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, 0);
	if (!win)
		fatal_sdl_error("SDL_CreateWindow");

	SDL_Surface *winsurf = SDL_GetWindowSurface(win);
	if (!winsurf)
		fatal_sdl_error("SDL_GetWindowSurface");

	struct GameState gs = {0};
	gs.players[0].sphere = sphere_load("person1.png", (struct Vec3){0,0.5f,-2});
	gs.players[1].sphere = sphere_load("person2.png", (struct Vec3){2,0.5f,-2});

	// This turned out to be much faster than blitting
	gs.players[0].cam.surface = create_half_surface(winsurf, 0, winsurf->w/2);
	gs.players[1].cam.surface = create_half_surface(winsurf, winsurf->w/2, winsurf->w/2);
	if (!gs.players[0].cam.surface || !gs.players[1].cam.surface)
		fatal_sdl_error("SDL_CreateRGBSurface");

	player_updatecam(&gs.players[0]);
	player_updatecam(&gs.players[1]);

	uint32_t time = 0;
	while(1){
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if (!handle_event(event, &gs))
				goto exit;
		}

		player_turn(&gs.players[0], FPS);
		player_move(&gs.players[0], FPS);
		player_turn(&gs.players[1], FPS);
		player_move(&gs.players[1], FPS);

		SDL_FillRect(winsurf, NULL, 0x000000UL);
		show_everything(&gs, &gs.players[0].cam);
		show_everything(&gs, &gs.players[1].cam);
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
	for (unsigned i = 0; i < 2; i++)
		free(gs.players[i].sphere);
	SDL_FreeSurface(gs.players[0].cam.surface);
	SDL_FreeSurface(gs.players[1].cam.surface);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
