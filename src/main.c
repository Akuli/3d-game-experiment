#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "display.h"
#include "sphere.h"
#include "vecmat.h"

#include <SDL2/SDL.h>

#define FPS 30

int main(void)
{
	SDL_Window *win;
	SDL_Renderer *rnd;

	if (SDL_CreateWindowAndRenderer(DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, &win, &rnd) == -1)
		fatal_sdl_error("SDL_CreateWindowAndRenderer");

	struct Sphere *sph = sphere_load("person.png", (struct Vec3){0,0,-100});

	uint32_t time = 0;
	while(1){
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT ||
					(event.type == SDL_KEYDOWN &&
					event.key.keysym.scancode == SDL_SCANCODE_ESCAPE))
			{
				goto exit;
			}
		}

		SDL_SetRenderDrawColor(rnd, 0, 0, 0, 0xff);
		SDL_RenderFillRect(rnd, NULL);

		sphere_display(sph, rnd);
		SDL_RenderPresent(rnd);

		uint32_t curtime = SDL_GetTicks();
		fprintf(stderr, "speed percentage thingy = %.1f%%\n",
			(float)(curtime - time) / (1000/FPS) * 100.f);

		time += 1000/FPS;
		if (curtime <= time) {
			SDL_Delay(time - curtime);
		} else {
			fprintf(stderr,
				"an iteration of the main loop is %dms late. "
				"Each iteration should last %dms. "
				"Game started %.2fs ago.\n",
				(unsigned)(curtime - time),
				1000/FPS,
				(float)curtime/1000);
			time = curtime;
		}
	}

exit:
	free(sph);
	SDL_DestroyWindow(win);
	SDL_DestroyRenderer(rnd);
	SDL_Quit();
	return 0;
}
