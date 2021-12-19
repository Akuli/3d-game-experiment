#ifndef MAX_H
#define MAX_H

// the players can have INT_MAX picked guards or whatever, we display only some of them
#define MAX_PICKED_GUARDS_TO_DISPLAY_PER_PLAYER 64

#define MAX_UNPICKED_GUARDS 1024        // big max just for fun
#define MAX_ENEMIES 256
#define MAX_ELLIPSOIDS ( \
	MAX_ENEMIES \
	+ MAX_UNPICKED_GUARDS \
	+ (\
		1                                          /* player */ \
		+ MAX_PICKED_GUARDS_TO_DISPLAY_PER_PLAYER  /* and his/her guards */ \
	)*2                                            /* for both players */ \
)

#define MAX_MAPSIZE 30
#define MAX_WALLS (2*MAX_MAPSIZE*(MAX_MAPSIZE + 1))  // TODO: make smaller?
#define MAX_RECTS MAX_WALLS

#endif    // MAX_H
