#include "looptimer.h"
#include "camera.h"
#include "log.h"
#include <SDL2/SDL.h>

void looptimer_wait(struct LoopTimer *lt)
{
	uint32_t curtime = SDL_GetTicks();   // milliseconds
	if (lt->time == 0) {
		// first time
		lt->time = curtime;
		return;
	}

	// For whatever reason, this happens sometimes in game over screen.
	if (curtime < lt->time)
		curtime = lt->time;

	float percent = (float)(curtime - lt->time) / (1000/CAMERA_FPS) * 100.f;
	lt->percentsum += percent;
	++lt->percentcount;
	if (lt->percentcount == CAMERA_FPS/3) {
		log_printf("speed percentage average = %.2f%%", lt->percentsum / (float)lt->percentcount);
		lt->percentcount = 0;
		lt->percentsum = 0;
	}

	lt->time += 1000/CAMERA_FPS;
	if (curtime <= lt->time) {
		SDL_Delay(lt->time - curtime);
	} else {
		lt->time = curtime;
		log_printf("event loop is lagging with speed percentage %.2f%%", percent);
	}
}
