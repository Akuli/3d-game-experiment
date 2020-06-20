#include "gameover.h"
#include <SDL2/SDL.h>
#include "log.h"
#include "player.h"

enum MiscState game_over(
	struct SDL_Window *wnd, const struct EllipsoidPic *winnerpic)
{
	log_printf("Winner is: %s", player_getname(winnerpic));
	return MISC_STATE_CHOOSER;
}
