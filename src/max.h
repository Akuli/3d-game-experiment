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

#define MAX_PLACE_SIZE 30
#define MAX_WALLS 1024

#endif    // MAX_H
