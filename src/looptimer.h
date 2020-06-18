// use this for event loops that run FPS times per second (or less if lagging)
#ifndef LOOPTIMER_H
#define LOOPTIMER_H

#include <stdint.h>

// initialize it like this:   struct LoopTimer lt = {0};
struct LoopTimer {
	uint32_t time;   // milliseconds since SDL_Init()

	// for logging how many % of the time it runs on average
	float percentsum;
	int percentcount;
};

// call this in your event loop
void looptimer_wait(struct LoopTimer *lt);

#endif   // LOOPTIMER_H
