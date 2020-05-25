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

#define FPS 30

// returns whether to continue playing
static bool handle_event(SDL_Event event, struct Player *plr)
{
	switch(event.type) {
	case SDL_QUIT:
		return false;

	case SDL_KEYDOWN:
		switch(event.key.keysym.scancode) {
		case SDL_SCANCODE_ESCAPE:
			return false;
		case SDL_SCANCODE_RIGHT:
			plr->turning = -1;
			break;
		case SDL_SCANCODE_LEFT:
			plr->turning = 1;
			break;
		case SDL_SCANCODE_UP:
			plr->moving = true;
			break;
		default:
			break;
		}
		break;

	case SDL_KEYUP:
		switch(event.key.keysym.scancode) {
		case SDL_SCANCODE_RIGHT:
		case SDL_SCANCODE_LEFT:
			plr->turning = 0;
			break;
		case SDL_SCANCODE_UP:
			plr->moving = false;
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


int main(void)
{
	SDL_Window *win;
	SDL_Renderer *rnd;

	if (SDL_CreateWindowAndRenderer(DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, &win, &rnd) == -1)
		fatal_sdl_error("SDL_CreateWindowAndRenderer");

	struct Player plr = {
		.sphere = sphere_load("person.png", (struct Vec3){0,0.5f,-2}),
		.cam = {
			.location = { 0,1,0 },
			.world2cam = { .rows = {
				{1, 0, 0},
				{0, 1, 0},
				{0, 0, 1},
			}},
			.renderer = rnd,
		},
		.turning = 0,
	};

	uint32_t time = 0;
	while(1){
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if (!handle_event(event, &plr))
				goto exit;
		}

		player_turn(&plr, FPS);
		player_move(&plr, FPS);

		SDL_SetRenderDrawColor(rnd, 0, 0, 0, 0xff);
		SDL_RenderFillRect(rnd, NULL);

		SDL_SetRenderDrawColor(rnd, 0xff, 0xff, 0, 0xff);
		for (float x = -10; x <= 10; x += 1.f) {
			for (float z = -10; z <= 0; z += 0.3f) {
				struct Vec3 worldvec = camera_point_world2cam(&plr.cam, (struct Vec3){ x, 0, z });
				if (worldvec.z < 0) {
					SDL_Point p = display_point_to_sdl(worldvec);
					SDL_RenderDrawPoint(rnd, p.x, p.y);
				}
			}
		}

		sphere_display(plr.sphere, &plr.cam);
		SDL_RenderPresent(rnd);

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
	free(plr.sphere);
	SDL_DestroyWindow(win);
	SDL_DestroyRenderer(rnd);
	SDL_Quit();
	return 0;
}
