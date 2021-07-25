#ifndef MAPEDITOR_H
#define MAPEDITOR_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "map.h"
#include "misc.h"

struct MapEditor;  // IWYU pragma: keep

// free() the return value when done
struct MapEditor *mapeditor_new(SDL_Surface *surf, int ytop, float zoom);

// Call these after mapeditor_new()
void mapeditor_setplayers(struct MapEditor *ed, const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic);
void mapeditor_setmap(struct MapEditor *ed, struct Map *map);

// Useful if you want to use MapEditor to display a map, but not actually edit it
void mapeditor_displayonly_eachframe(struct MapEditor *ed);

// Surface passed to mapeditor_new() must be SDL_GetWindowSurface(wnd)
enum MiscState mapeditor_run(struct MapEditor *ed, SDL_Window *wnd);

#endif   // MAPEDITOR_H
