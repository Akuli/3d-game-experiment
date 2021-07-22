#ifndef MAPEDITOR_H
#define MAPEDITOR_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "map.h"
#include "misc.h"

struct MapEditor;

// Use free() to destroy the map editor when done
// Needs array of all maps because it's possible to delete a map
struct MapEditor *mapeditor_new(
	SDL_Surface *surf,
	struct Map *maps, int *nmaps, int mapidx,
	const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic);

// You can disable event handling if you want to use map editor only to display a map
// Return value meaningful only when event handling enabled
enum MiscState mapeditor_eachframe(struct MapEditor *ed, bool eventhandling);

enum MiscState mapeditor_run(struct MapEditor *ed, SDL_Window *wnd);

#endif   // MAPEDITOR_H
