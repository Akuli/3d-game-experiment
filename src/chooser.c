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

// actual button size is smaller than this, which means we get some padding
#define BUTTON_SIZE 50

struct Button {
	// first draw image 1, then image 2 if any, then add text if any
	const char *img1path, *img2path, *text;
	SDL_Surface *cachesurf;   // same size as image 1
	SDL_Surface *destsurf;    // usually bigger than the button itself
	SDL_Point center;         // where to put button
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
	/*
	Hard-coding font size shoudn't matter much because font and buttons are the
	same on all operating systems.
	*/
	TTF_Font *font = TTF_OpenFont("DejaVuSans.ttf", 30);
	if (!font)
		log_printf_abort("TTF_OpenFont failed: %s", TTF_GetError());

	SDL_Color col = { 0, 0, 0, 0xff };

	SDL_Surface *s = TTF_RenderUTF8_Solid(font, text, col);
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

void refresh_button(struct Button *butt)
{
	if (butt->cachesurf)
		SDL_FreeSurface(butt->cachesurf);

	butt->cachesurf = create_image_surface(butt->img1path);

	if (butt->img2path) {
		SDL_Surface *s = create_image_surface(butt->img2path);
		blit_with_center(s, butt->cachesurf, NULL);
		SDL_FreeSurface(s);
	}

	if (butt->text && butt->text[0]) {
		SDL_Surface *s = create_text_surface(butt->text);
		blit_with_center(s, butt->cachesurf, NULL);
		SDL_FreeSurface(s);
	}
}

static void show_button(const struct Button *butt)
{
	blit_with_center(butt->cachesurf, butt->destsurf, &butt->center);
}

#define PLAYER_CHOOSER_HEIGHT ( CAMERA_SCREEN_HEIGHT/2 )
#define PLACE_CHOOSER_HEIGHT (CAMERA_SCREEN_HEIGHT - PLAYER_CHOOSER_HEIGHT)

struct PlayerChooser {
	int index;
	float anglediff;    // how much the player chooser is about to spin
	struct Button prevbtn, nextbtn;
	struct Camera cam;
};

struct ChooserState {
	struct PlayerChooser playerch1, playerch2;
	struct Ellipsoid ellipsoids[FILELIST_NPLAYERS];
};

static void calculate_player_chooser_geometry_stuff(
	int leftx, SDL_Rect *preview, SDL_Point *prevbcenter, SDL_Point *nextbcenter)
{
	preview->w = CAMERA_SCREEN_WIDTH/2 - 2*BUTTON_SIZE;
	preview->h = PLAYER_CHOOSER_HEIGHT;
	preview->x = leftx + CAMERA_SCREEN_WIDTH/4 - preview->w/2;
	preview->y = 0;

	prevbcenter->x = leftx + BUTTON_SIZE/2;
	nextbcenter->x = leftx + CAMERA_SCREEN_WIDTH/2 - BUTTON_SIZE/2;
	prevbcenter->y = PLAYER_CHOOSER_HEIGHT/2;
	nextbcenter->y = PLAYER_CHOOSER_HEIGHT/2;
}

static void setup_player_chooser(struct PlayerChooser *ch, int idx, int centerx, SDL_Surface *surf)
{
	SDL_Rect preview;
	SDL_Point prevc, nextc;
	calculate_player_chooser_geometry_stuff(centerx, &preview, &prevc, &nextc);

	float pi = acosf(-1);
	*ch = (struct PlayerChooser){
		.index = idx,
		.prevbtn = {
			.img1path = "buttons/small/vertical/normal.png",
			.img2path = "arrows/left.png",
			.destsurf = surf,
			.center = prevc,
		},
		.nextbtn = {
			.img1path = "buttons/small/vertical/normal.png",
			.img2path = "arrows/right.png",
			.destsurf = surf,
			.center = nextc,
		},
		.cam = {
			.screencentery = 0.2f*PLAYER_CHOOSER_HEIGHT,
			.surface = camera_create_cropped_surface(surf, preview),
			.angle = (2*pi)/FILELIST_NPLAYERS * idx,
		},
	};

	refresh_button(&ch->prevbtn);
	refresh_button(&ch->nextbtn);
}

static void set_button_pressed(struct Button *butt, bool pressed)
{
	if (pressed)
		butt->img1path = "buttons/small/vertical/pressed.png";
	else
		butt->img1path = "buttons/small/vertical/normal.png";
	refresh_button(butt);
}

static void rotate_player_chooser(struct PlayerChooser *ch, int dir)
{
	assert(dir == +1 || dir == -1);
	float pi = acosf(-1);

	ch->index -= dir;   // don't know why subtracting here helps...
	ch->index %= FILELIST_NPLAYERS;
	if (ch->index < 0)
		ch->index += FILELIST_NPLAYERS;
	assert(0 <= ch->index && ch->index < FILELIST_NPLAYERS);

	// why subtracting: more angle = clockwise from above = left in chooser
	ch->anglediff -= dir * (2*pi) / (float)FILELIST_NPLAYERS;

	set_button_pressed(&ch->prevbtn, false);
	set_button_pressed(&ch->nextbtn, false);
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

enum HandleEventResult { QUIT, PLAY, CONTINUE };
static enum HandleEventResult handle_event(const SDL_Event event, struct ChooserState *st)
{
	switch(event.type) {
	case SDL_QUIT:
		return QUIT;

	case SDL_KEYDOWN:
		switch(event.key.keysym.scancode) {
			case SDL_SCANCODE_A:     set_button_pressed(&st->playerch1.prevbtn, true); break;
			case SDL_SCANCODE_D:     set_button_pressed(&st->playerch1.nextbtn, true); break;
			case SDL_SCANCODE_LEFT:  set_button_pressed(&st->playerch2.prevbtn, true); break;
			case SDL_SCANCODE_RIGHT: set_button_pressed(&st->playerch2.nextbtn, true); break;

			// FIXME: add big "Play" button instead of this
			case SDL_SCANCODE_RETURN: return PLAY;
			default: break;
		}
		break;

	case SDL_KEYUP:
		switch(event.key.keysym.scancode) {
			case SDL_SCANCODE_A:     rotate_player_chooser(&st->playerch1, -1); break;
			case SDL_SCANCODE_D:     rotate_player_chooser(&st->playerch1, +1); break;
			case SDL_SCANCODE_LEFT:  rotate_player_chooser(&st->playerch2, -1); break;
			case SDL_SCANCODE_RIGHT: rotate_player_chooser(&st->playerch2, +1); break;
			default: break;
		}
		break;

	default: break;
	}

	return CONTINUE;
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
		float angle = pi/2 + ( i/(float)FILELIST_NPLAYERS * (2*pi) );

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

bool chooser_run(
	SDL_Window *win,
	const struct EllipsoidPic **plr1pic, const struct EllipsoidPic **plr2pic,
	const struct Place **pl)
{
	SDL_Surface *winsurf = SDL_GetWindowSurface(win);
	if (!winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());
	SDL_FillRect(winsurf, NULL, 0);

	// TODO: implement a place chooser
	*pl = &place_list()[0];

	static struct ChooserState st;
	create_player_ellipsoids(st.ellipsoids, winsurf->format);

	setup_player_chooser(&st.playerch1, 0, 0, winsurf);
	setup_player_chooser(&st.playerch2, 1, CAMERA_SCREEN_WIDTH/2, winsurf);

	struct LoopTimer lt = {0};

	while(1) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			enum HandleEventResult r = handle_event(e, &st);
			switch(r) {
			case PLAY:
			case QUIT:
				free_player_chooser(&st.playerch1);
				free_player_chooser(&st.playerch2);

				*plr1pic = &player_get_epics(winsurf->format)[st.playerch1.index];
				*plr2pic = &player_get_epics(winsurf->format)[st.playerch2.index];

				return (r == PLAY);
			case CONTINUE:
				break;
			}
		}

		for (int i = 0; i < FILELIST_NPLAYERS; i++) {
			st.ellipsoids[i].angle += 1.0f / CAMERA_FPS;
			ellipsoid_update_transforms(&st.ellipsoids[i]);
		}

		turn_camera(&st.playerch1);
		turn_camera(&st.playerch2);

		SDL_FillRect(winsurf, NULL, 0);
		show_all(NULL, 0, st.ellipsoids, FILELIST_NPLAYERS, &st.playerch1.cam);
		show_all(NULL, 0, st.ellipsoids, FILELIST_NPLAYERS, &st.playerch2.cam);
		show_button(&st.playerch1.prevbtn);
		show_button(&st.playerch1.nextbtn);
		show_button(&st.playerch2.prevbtn);
		show_button(&st.playerch2.nextbtn);

		SDL_UpdateWindowSurface(win);
		looptimer_wait(&lt);
	}
}
