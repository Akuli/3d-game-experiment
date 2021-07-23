#ifndef MAPEDITOR_H
#define MAPEDITOR_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "map.h"
#include "misc.h"

struct MapEditor;  // IWYU pragma: keep

// You also need to call mapeditor_setmaps and mapeditor_setplayers
// free() the return value when done but not between calls
struct MapEditor *mapeditor_new(SDL_Surface *surf, int ytop);

void mapeditor_setplayers(struct MapEditor *ed, const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic);

// Call this when current map or the array of all maps changes
// Needs array of all maps because it's possible to delete a map
void mapeditor_setmaps(struct MapEditor *ed, struct Map *maps, int *nmaps, int mapidx);

// Useful if you want to use MapEditor to display a map, but not actually edit it
void mapeditor_displayonly_eachframe(struct MapEditor *ed);

// Surface passed to mapeditor_new() must be SDL_GetWindowSurface(wnd)
enum MiscState mapeditor_run(struct MapEditor *ed, SDL_Window *wnd);

#endif   // MAPEDITOR_H
