#include "chooser.h"
#include "../generated/filelist.h"
#include "camera.h"
#include "ellipsoid.h"
#include "looptimer.h"
#include "mathstuff.h"
#include "place.h"
#include "player.h"
#include "showall.h"
#include "../stb/stb_image.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// actual button sizes are smaller, we get padding
#define SMALL_BUTTON_SIZE 50
#define BIG_BUTTON_SIZE 350
#define TEXT_SIZE 40

struct PlayerChooser;

struct Button {
	// imgpath image and text are drawn on top of a generic button background image
	const char *imgpath;
	const char *text;

	bool big;
	bool horizontal;
	bool pressed;

	// which keyboard key press corresponds to this button?
	int scancode;

	// cachesurf is blitted to destsurf on each frame
	SDL_Surface *cachesurf;
	SDL_Surface *destsurf;
	SDL_Point center;

	void (*onclick)(void *onclickdata);
	void *onclickdata;
};

static void blit_with_center(SDL_Surface *src, SDL_Surface *dst, const SDL_Point *center)
{
	int cx, cy;
	if (center) {
		cx = center->x;
		cy = center->y;
	} else {
		cx = dst->w/2;
		cy = dst->h/2;
	}
	SDL_Rect r = { cx - src->w/2, cy - src->h/2, src->w, src->h };
	SDL_BlitSurface(src, NULL, dst, &r);
}

static SDL_Surface *create_text_surface(const char *text)
{
	TTF_Font *font = TTF_OpenFont("DejaVuSans.ttf", TEXT_SIZE);
	if (!font)
		log_printf_abort("TTF_OpenFont failed: %s", TTF_GetError());

	SDL_Color col = { 0xff, 0xff, 0xff, 0xff };

	SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, col);
	TTF_CloseFont(font);
	if (!s)
		log_printf_abort("TTF_RenderUTF8_Solid failed: %s", TTF_GetError());
	return s;
}

static SDL_Surface *create_image_surface(const char *path)
{
	int fmt, w, h;
	unsigned char *data = stbi_load(path, &w, &h, &fmt, 4);
	if (!data)
		log_printf_abort("loading image from '%s' failed: %s", path, stbi_failure_reason());

	// SDL_CreateRGBSurfaceWithFormatFrom docs have example code for using it with stbi :D
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(
		data, w, h, 32, 4*w, SDL_PIXELFORMAT_RGBA32);
	if (!s)
		log_printf_abort("SDL_CreateRGBSurfaceWithFormatFrom failed: %s", SDL_GetError());
	return s;
}

static void refresh_button(struct Button *butt)
{
	if (butt->cachesurf)
		SDL_FreeSurface(butt->cachesurf);

	char path[100];
	snprintf(path, sizeof path, "buttons/%s/%s/%s",
		butt->big ? "big" : "small",
		butt->horizontal ? "horizontal" : "vertical",
		butt->pressed ? "pressed.png" : "normal.png");
	butt->cachesurf = create_image_surface(path);

	if (butt->imgpath) {
		SDL_Surface *s = create_image_surface(butt->imgpath);
		blit_with_center(s, butt->cachesurf, NULL);
		SDL_FreeSurface(s);
	}

	if (butt->text) {
		SDL_Surface *s = create_text_surface(butt->text);
		blit_with_center(s, butt->cachesurf, NULL);
		SDL_FreeSurface(s);
	}
}

static void show_button(const struct Button *butt)
{
	blit_with_center(butt->cachesurf, butt->destsurf, &butt->center);
}

static bool mouse_on_button(const SDL_MouseButtonEvent *me, const struct Button *butt)
{
	return fabsf(me->x - butt->center.x) < butt->cachesurf->w/2 &&
			fabsf(me->y - butt->center.y) < butt->cachesurf->h/2;
}

static void handle_button_event(const SDL_Event *evt, struct Button *butt)
{
	if ((
		(evt->type == SDL_MOUSEBUTTONDOWN && mouse_on_button(&evt->button, butt)) ||
		(evt->type == SDL_KEYDOWN && evt->key.keysym.scancode == butt->scancode)
	) && !butt->pressed) {
		butt->pressed = true;
	} else if ((
		(evt->type == SDL_MOUSEBUTTONUP && mouse_on_button(&evt->button, butt)) ||
		(evt->type == SDL_KEYUP && evt->key.keysym.scancode == butt->scancode)
	) && butt->pressed) {
		butt->pressed = false;
		butt->onclick(butt->onclickdata);
	} else if (evt->type == SDL_MOUSEBUTTONUP && butt->pressed) {
		// if button has been pressed and mouse has been moved away, unpress button but don't click
		butt->pressed = false;
	} else {
		// don't refresh
		return;
	}
	refresh_button(butt);
}

#define PLAYER_CHOOSER_HEIGHT ( 0.4f*CAMERA_SCREEN_HEIGHT )
#define PLACE_CHOOSER_HEIGHT (CAMERA_SCREEN_HEIGHT - PLAYER_CHOOSER_HEIGHT)

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
	preview->y = TEXT_SIZE;

	prevbcenter->x = leftx + SMALL_BUTTON_SIZE/2;
	nextbcenter->x = leftx + CAMERA_SCREEN_WIDTH/2 - SMALL_BUTTON_SIZE/2;
	prevbcenter->y = TEXT_SIZE + PLAYER_CHOOSER_HEIGHT/2;
	nextbcenter->y = TEXT_SIZE + PLAYER_CHOOSER_HEIGHT/2;
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

	ch->nametextsurf = create_text_surface(name);
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
			.surface = camera_create_cropped_surface(surf, preview),
			.angle = -(2*pi)/FILELIST_NPLAYERS * idx,
		},
	};

	ch->prevbtn.onclickdata = ch;
	ch->nextbtn.onclickdata = ch;

	set_player_chooser_index(ch, idx);
	refresh_button(&ch->prevbtn);
	refresh_button(&ch->nextbtn);
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
	switch(evt->type) {
	case SDL_QUIT:
		return false;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		handle_button_event(evt, &st->playerch1.prevbtn);
		handle_button_event(evt, &st->playerch1.nextbtn);
		handle_button_event(evt, &st->playerch2.prevbtn);
		handle_button_event(evt, &st->playerch2.nextbtn);
		handle_button_event(evt, &st->bigplaybtn);

	default: break;
	}
	return true;
}

static void show_player_chooser(const struct ChooserState *st, const struct PlayerChooser *ch)
{
	show_all(NULL, 0, st->ellipsoids, FILELIST_NPLAYERS, &ch->cam);
	show_button(&ch->prevbtn);
	show_button(&ch->nextbtn);
	blit_with_center(ch->nametextsurf, st->winsurf, &(SDL_Point){
		ch->leftx + st->winsurf->w/4,
		TEXT_SIZE + PLAYER_CHOOSER_HEIGHT + TEXT_SIZE/2,
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
	refresh_button(&st.bigplaybtn);

	// TODO: implement a place chooser
	*pl = &place_list()[0];

	create_player_ellipsoids(st.ellipsoids, st.winsurf->format);
	setup_player_chooser(&st.playerch1, st.winsurf, 0, 0, SDL_SCANCODE_A, SDL_SCANCODE_D);
	setup_player_chooser(&st.playerch2, st.winsurf, 1, CAMERA_SCREEN_WIDTH/2, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT);
	SDL_Surface *chooseplrtxt = create_text_surface("Chooser players:");

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
		blit_with_center(chooseplrtxt, st.winsurf, &(SDL_Point){st.winsurf->w/2, TEXT_SIZE/2});
		show_player_chooser(&st, &st.playerch1);
		show_player_chooser(&st, &st.playerch2);
		show_button(&st.bigplaybtn);

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
