#ifndef MAPEDITOR_H
#define MAPEDITOR_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "map.h"
#include "misc.h"

struct MapEditor;

// Can be called several times, free() the return value when done but not between calls
// Needs array of all maps because it's possible to delete a map
struct MapEditor *mapeditor_new(
	struct MapEditor *ed,  // NULL when called for the first time, otherwise previous return value
	SDL_Surface *surf, int ytop,
	struct Map *maps, int *nmaps, int mapidx,
	const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic);

// Useful if you want to use MapEditor to display a map, but not actually edit it
void mapeditor_displayonly_eachframe(struct MapEditor *ed);

// If you want to change the map, you need to make a new map editor
void mapeditor_setplayers(struct MapEditor *ed, const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic);

// Surface passed to mapeditor_new() must be SDL_GetWindowSurface(wnd)
enum MiscState mapeditor_run(struct MapEditor *ed, SDL_Window *wnd);

#endif   // MAPEDITOR_H
