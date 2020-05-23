#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#include "display.h"
#include "sphere.h"
#include "vecmat.h"

#include <SDL2/SDL.h>

#define FPS 15

noreturn static void fatal_error(const char *whatfailed, const char *msg)
{
	// TODO: write to log somewhere?
	if (msg)
		fprintf(stderr, "%s failed: %s\n", whatfailed, msg);
	else
		fprintf(stderr, "%s failed\n", whatfailed);
	abort();
}

noreturn static void fatal_sdl_error(const char *whatfailed)
{
	fatal_error(whatfailed, SDL_GetError());
}

int main(void)
{
	SDL_Window *win;
	SDL_Renderer *rnd;

	if (SDL_CreateWindowAndRenderer(DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, &win, &rnd) == -1)
		fatal_sdl_error("SDL_CreateWindowAndRenderer");

	struct Sphere sph = { .center = { .x=0, .y=0, .z=-100 }, .radius = 1 };

	uint32_t time = 0;
	while(1){
		printf("loop\n");
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

		for (int x = 0; x < DISPLAY_WIDTH; x++) {
			for (int y = 0; y < DISPLAY_HEIGHT; y++) {
				struct DisplayLine ln = displayline_frompixel(x, y);
				if (!sphere_touches_displayline(sph, ln)) {
					//printf("no touch %d %d\n", x, y);
					continue;
				}

				SDL_SetRenderDrawColor(rnd, 0xff, 0, 0, 0xff);
				SDL_RenderDrawPoint(rnd, x, y);
			}
		}
		SDL_RenderPresent(rnd);

		uint32_t curtime = SDL_GetTicks();
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
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
