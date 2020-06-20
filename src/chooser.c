#include "chooser.h"
#include "../generated/filelist.h"
#include "button.h"
#include "camera.h"
#include "ellipsoid.h"
#include "looptimer.h"
#include "mathstuff.h"
#include "misc.h"
#include "place.h"
#include "player.h"
#include "showall.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// actual button sizes are smaller, we get padding
#define SMALL_BUTTON_SIZE 50
#define BIG_BUTTON_SIZE 350

#define PLAYER_CHOOSER_HEIGHT ( 0.4f*CAMERA_SCREEN_HEIGHT )
#define PLACE_CHOOSER_HEIGHT (CAMERA_SCREEN_HEIGHT - PLAYER_CHOOSER_HEIGHT)

static const SDL_Color white_color = { 0xff, 0xff, 0xff, 0xff };

struct PlayerChooser {
	int leftx;
	int index;
	float anglediff;    // how much the player chooser is about to spin
	struct Button prevbtn, nextbtn;
	struct Camera cam;
	SDL_Surface *nametextsurf;
};

struct ChooserState {
	SDL_Surface *winsurf;
	struct PlayerChooser playerch1, playerch2;
	struct Ellipsoid ellipsoids[FILELIST_NPLAYERS];
	struct Button bigplaybtn;
};

static void calculate_player_chooser_geometry_stuff(
	int leftx, SDL_Rect *preview, SDL_Point *prevbcenter, SDL_Point *nextbcenter)
{
	preview->w = CAMERA_SCREEN_WIDTH/2 - 2*SMALL_BUTTON_SIZE;
	preview->h = PLAYER_CHOOSER_HEIGHT;
	preview->x = leftx + CAMERA_SCREEN_WIDTH/4 - preview->w/2;
	preview->y = MISC_TEXT_SIZE;

	prevbcenter->x = leftx + SMALL_BUTTON_SIZE/2;
	nextbcenter->x = leftx + CAMERA_SCREEN_WIDTH/2 - SMALL_BUTTON_SIZE/2;
	prevbcenter->y = MISC_TEXT_SIZE + PLAYER_CHOOSER_HEIGHT/2;
	nextbcenter->y = MISC_TEXT_SIZE + PLAYER_CHOOSER_HEIGHT/2;
}

static void set_player_chooser_index(struct PlayerChooser *ch, int idx)
{
	idx %= FILELIST_NPLAYERS;
	if (idx < 0)
		idx += FILELIST_NPLAYERS;
	assert(0 <= idx && idx < FILELIST_NPLAYERS);
	ch->index = idx;

	if (ch->nametextsurf)
		SDL_FreeSurface(ch->nametextsurf);

	const char *path = filelist_players[idx];

	const char *prefix = "players/";
	assert(strstr(path, prefix) == path);
	path += strlen(prefix);

	char name[100] = {0};
	strncpy(name, path, sizeof(name)-1);
	char *dot = strrchr(name, '.');
	assert(dot);
	*dot = '\0';

	ch->nametextsurf = misc_create_text_surface(name, white_color);
}

static void rotate_player_chooser(struct PlayerChooser *ch, int dir)
{
	assert(dir == +1 || dir == -1);
	set_player_chooser_index(ch, ch->index + dir);
	float pi = acosf(-1);

	// why subtracting: more angle = clockwise from above = left in chooser
	ch->anglediff -= dir * (2*pi) / (float)FILELIST_NPLAYERS;
}

static void rotate_left (void *playerch) { rotate_player_chooser(playerch, -1); }
static void rotate_right(void *playerch) { rotate_player_chooser(playerch, +1); }

static void setup_player_chooser(struct PlayerChooser *ch, SDL_Surface *surf, int idx, int leftx, int scprev, int scnext)
{
	SDL_Rect preview;
	SDL_Point prevc, nextc;
	calculate_player_chooser_geometry_stuff(leftx, &preview, &prevc, &nextc);

	float pi = acosf(-1);
	*ch = (struct PlayerChooser){
		.leftx = leftx,
		.prevbtn = {
			.imgpath = "arrows/left.png",
			.scancode = scprev,
			.destsurf = surf,
			.center = prevc,
			.onclick = rotate_left,
		},
		.nextbtn = {
			.imgpath = "arrows/right.png",
			.scancode = scnext,
			.destsurf = surf,
			.center = nextc,
			.onclick = rotate_right,
		},
		.cam = {
			.screencentery = 0,
			.surface = misc_create_cropped_surface(surf, preview),
			.angle = -(2*pi)/FILELIST_NPLAYERS * idx,
		},
	};

	ch->prevbtn.onclickdata = ch;
	ch->nextbtn.onclickdata = ch;

	set_player_chooser_index(ch, idx);
	button_refresh(&ch->prevbtn);
	button_refresh(&ch->nextbtn);
	camera_update_caches(&ch->cam);
}

static void turn_camera(struct PlayerChooser *ch)
{
	float maxturn = 50.0f / (CAMERA_FPS * FILELIST_NPLAYERS);
	assert(maxturn > 0);
	float turn = max(-maxturn, min(maxturn, ch->anglediff));

	ch->cam.angle += turn;
	ch->anglediff -= turn;

	ch->cam.location = mat3_mul_vec3(mat3_rotation_xz(ch->cam.angle), (Vec3){0,0.7f,3.0f});
	camera_update_caches(&ch->cam);
}

// returns false when game should quit
static bool handle_event(const SDL_Event *evt, struct ChooserState *st)
{
	if (evt->type == SDL_QUIT)
		return false;

	button_handle_event(evt, &st->playerch1.prevbtn);
	button_handle_event(evt, &st->playerch1.nextbtn);
	button_handle_event(evt, &st->playerch2.prevbtn);
	button_handle_event(evt, &st->playerch2.nextbtn);
	button_handle_event(evt, &st->bigplaybtn);
	return true;
}

static void show_player_chooser(const struct ChooserState *st, const struct PlayerChooser *ch)
{
	show_all(NULL, 0, st->ellipsoids, FILELIST_NPLAYERS, &ch->cam);
	button_show(&ch->prevbtn);
	button_show(&ch->nextbtn);
	misc_blit_with_center(ch->nametextsurf, st->winsurf, &(SDL_Point){
		ch->leftx + st->winsurf->w/4,
		MISC_TEXT_SIZE + PLAYER_CHOOSER_HEIGHT + MISC_TEXT_SIZE/2,
	});
}

static void free_player_chooser(const struct PlayerChooser *ch)
{
	SDL_FreeSurface(ch->cam.surface);
	SDL_FreeSurface(ch->prevbtn.cachesurf);
	SDL_FreeSurface(ch->nextbtn.cachesurf);
}

static void create_player_ellipsoids(struct Ellipsoid *arr, const SDL_PixelFormat *fmt)
{
	float pi = acosf(-1);

	for (int i = 0; i < FILELIST_NPLAYERS; i++) {
		// pi/2 to make first players (i=0 and i=1) look at camera
		float angle = pi/2 - ( i/(float)FILELIST_NPLAYERS * (2*pi) );

		arr[i] = (struct Ellipsoid){
			.epic = &player_get_epics(fmt)[i],
			.center = mat3_mul_vec3(mat3_rotation_xz(angle), (Vec3){ 1.4f, 0, 0 }),
			.angle = angle,
			.xzradius = PLAYER_XZRADIUS,
			.yradius = PLAYER_YRADIUS_NOFLAT,
		};
		ellipsoid_update_transforms(&arr[i]);
	}
}

static void on_play_button_clicked(void *ptr)
{
	*(bool *)ptr = true;
}

bool chooser_run(
	SDL_Window *win,
	const struct EllipsoidPic **plr1epic, const struct EllipsoidPic **plr2epic,
	const struct Place **pl)
{
	bool playbtnclicked = false;
	struct ChooserState st = {
		.winsurf = SDL_GetWindowSurface(win),
		.bigplaybtn = {
			.text = "Play",
			.big = true,
			.horizontal = true,
			.scancode = SDL_SCANCODE_RETURN,
			.destsurf = st.winsurf,
			.center = { CAMERA_SCREEN_WIDTH/2, (CAMERA_SCREEN_HEIGHT + PLAYER_CHOOSER_HEIGHT)/2 },
			.onclick = on_play_button_clicked,
			.onclickdata = &playbtnclicked,
		},
	};

	if (!st.winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());
	button_refresh(&st.bigplaybtn);

	// TODO: implement a place chooser
	*pl = &place_list()[0];

	create_player_ellipsoids(st.ellipsoids, st.winsurf->format);
	setup_player_chooser(&st.playerch1, st.winsurf, 0, 0, SDL_SCANCODE_A, SDL_SCANCODE_D);
	setup_player_chooser(&st.playerch2, st.winsurf, 1, CAMERA_SCREEN_WIDTH/2, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT);

	SDL_Surface *chooseplrtxt = misc_create_text_surface("Chooser players:", white_color);

	struct LoopTimer lt = {0};

	while(!playbtnclicked) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (!handle_event(&e, &st))
				goto out;
		}

		for (int i = 0; i < FILELIST_NPLAYERS; i++) {
			st.ellipsoids[i].angle += 1.0f / CAMERA_FPS;
			ellipsoid_update_transforms(&st.ellipsoids[i]);
		}

		turn_camera(&st.playerch1);
		turn_camera(&st.playerch2);

		SDL_FillRect(st.winsurf, NULL, 0);
		misc_blit_with_center(chooseplrtxt, st.winsurf, &(SDL_Point){st.winsurf->w/2, MISC_TEXT_SIZE/2});
		show_player_chooser(&st, &st.playerch1);
		show_player_chooser(&st, &st.playerch2);
		button_show(&st.bigplaybtn);

		SDL_UpdateWindowSurface(win);
		looptimer_wait(&lt);
	}

out:
	free_player_chooser(&st.playerch1);
	free_player_chooser(&st.playerch2);
	*plr1epic = &player_get_epics(st.winsurf->format)[st.playerch1.index];
	*plr2epic = &player_get_epics(st.winsurf->format)[st.playerch2.index];
	return playbtnclicked;
}
