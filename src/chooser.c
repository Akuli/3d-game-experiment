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
#define BUTTON_SIZE 60

struct Button {
	const char *imgpath;
	SDL_Surface *surface;   // exactly size of button, contains image
	SDL_Point center;       // where to put button
	const char *text;       // utf-8
};

static void show_text(SDL_Surface *surf, const char *text, SDL_Point center)
{
	/*
	Hard-coding font size shoudn't matter much because font and buttons are the
	same on all operating systems.
	*/
	TTF_Font *font = TTF_OpenFont("DejaVuSans.ttf", 30);
	if (!font)
		log_printf_abort("TTF_OpenFont failed: %s", TTF_GetError());

	SDL_Color col = { 0, 0, 0, 0xff };

	SDL_Surface *textsurf = TTF_RenderUTF8_Solid(font, text, col);
	TTF_CloseFont(font);
	if (!textsurf)
		log_printf_abort("TTF_RenderUTF8_Solid failed: %s", TTF_GetError());

	int x = center.x - textsurf->w/2;
	int y = center.y - textsurf->h/2;
	SDL_BlitSurface(textsurf, NULL, surf, &(SDL_Rect){ x, y, textsurf->w, textsurf->h });
	SDL_FreeSurface(textsurf);
}

// sets image size to *w and *h
static void set_button_image(struct Button *butt, const char *path)
{
	if (butt->imgpath != NULL && strcmp(path, butt->imgpath) == 0)
		return;

	butt->imgpath = path;

	int fmt, w, h;
	unsigned char *data = stbi_load(path, &w, &h, &fmt, 4);
	if (!data)
		log_printf_abort("loading button image from '%s' failed: %s", path, stbi_failure_reason());

	// SDL_CreateRGBSurfaceWithFormatFrom docs have example code for using it with stbi :D
	if (butt->surface)
		SDL_FreeSurface(butt->surface);

	butt->surface = SDL_CreateRGBSurfaceWithFormatFrom(
		data, w, h, 32, 4*w, SDL_PIXELFORMAT_RGBA32);
	if (!butt->surface)
		log_printf_abort("SDL_CreateRGBSurfaceWithFormatFrom failed: %s", SDL_GetError());

	show_text(butt->surface, butt->text, (SDL_Point){ w/2, h/2 });
}

static void show_button(const struct Button *butt, SDL_Surface *s)
{
	assert(butt->surface);
	SDL_BlitSurface(butt->surface, NULL, s, &(SDL_Rect){
		butt->center.x - butt->surface->w/2,
		butt->center.y - butt->surface->h/2,
		butt->surface->w,
		butt->surface->h,
	});
}

#define PLAYER_CHOOSER_HEIGHT ( CAMERA_SCREEN_HEIGHT*2/3 )
#define PLACE_CHOOSER_HEIGHT (CAMERA_SCREEN_HEIGHT - PLAYER_CHOOSER_HEIGHT)

struct PlayerChooser {
	int index;
	float anglediff;    // how much the player chooser is about to spin
	struct Button prevbtn, nextbtn;
	struct Camera cam;
};

struct ChooserState {
	struct PlayerChooser playerch1, playerch2;
	struct Ellipsoid ellipsoids[sizeof(filelist_players)/sizeof(filelist_players[0])];
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

	printf("prevc = %d %d\n", prevc.x, prevc.y);

	int nplayers = sizeof(filelist_players)/sizeof(filelist_players[0]);
	float pi = acosf(-1);
	*ch = (struct PlayerChooser){
		.index = idx,
		.prevbtn = { .text = "<", .center = prevc },
		.nextbtn = { .text = ">", .center = nextc },
		.cam = {
			.surface = camera_create_cropped_surface(surf, preview),
			.angle = (2*pi)/nplayers * idx,
		},
	};

	set_button_image(&ch->prevbtn, "buttons/small/normal.png");
	set_button_image(&ch->nextbtn, "buttons/small/normal.png");
}

static void rotate_player_chooser(struct PlayerChooser *pl, int dir)
{
	assert(dir == +1 || dir == -1);
	int nplayers = sizeof(filelist_players)/sizeof(filelist_players[0]);
	float pi = acosf(-1);

	pl->index -= dir;   // don't know why subtracting here helps...
	pl->index %= nplayers;
	if (pl->index < 0)
		pl->index += nplayers;
	assert(0 <= pl->index && pl->index < nplayers);

	// why subtracting: more angle = clockwise from above = left in chooser
	pl->anglediff -= dir * (2*pi) / (float)nplayers;
}

static void turn_camera(struct PlayerChooser *ch)
{
	float maxturn = 10.0f / CAMERA_FPS;
	assert(maxturn > 0);
	float turn = max(-maxturn, min(maxturn, ch->anglediff));

	ch->cam.angle += turn;
	ch->anglediff -= turn;

	ch->cam.location = mat3_mul_vec3(mat3_rotation_xz(ch->cam.angle), (Vec3){0,1,3});
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
			case SDL_SCANCODE_A:     rotate_player_chooser(&st->playerch1, -1); break;
			case SDL_SCANCODE_D:     rotate_player_chooser(&st->playerch1, +1); break;
			case SDL_SCANCODE_LEFT:  rotate_player_chooser(&st->playerch2, -1); break;
			case SDL_SCANCODE_RIGHT: rotate_player_chooser(&st->playerch2, +1); break;

			// FIXME: add big "Play" button instead of this
			case SDL_SCANCODE_P: return PLAY;
			default: break;
		}
		break;

	default: break;
	}

	return CONTINUE;
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

	// ellipsoids are laied out into a circle with radius r
	static const float r = 1;

	int nplayers = sizeof(st.ellipsoids)/sizeof(st.ellipsoids[0]);
	for (int i = 0; i < nplayers; i++) {
		float pi = acosf(-1);
		// initial angle at pi/2 so that players are about to turn to look at camera
		float angle = ( (float)i / nplayers * (2*pi) ) + pi/2;

		st.ellipsoids[i] = (struct Ellipsoid){
			.epic = &player_get_epics(winsurf->format)[i],
			.center = { r*cosf(angle), 0, r*sinf(angle) },
			.angle = angle,   // about to turn so that it looks at camera
			.xzradius = PLAYER_XZRADIUS,
			.yradius = PLAYER_YRADIUS_NOFLAT,
		};
		ellipsoid_update_transforms(&st.ellipsoids[i]);
	}

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
				// FIXME: free button surfaces
				SDL_FreeSurface(st.playerch1.cam.surface);
				SDL_FreeSurface(st.playerch2.cam.surface);

				*plr1pic = &player_get_epics(winsurf->format)[st.playerch1.index];
				*plr2pic = &player_get_epics(winsurf->format)[st.playerch2.index];

				return (r == PLAY);
			case CONTINUE:
				break;
			}
		}

		for (int i = 0; i < nplayers; i++) {
			st.ellipsoids[i].angle += 1.0f / CAMERA_FPS;
			ellipsoid_update_transforms(&st.ellipsoids[i]);
		}

		turn_camera(&st.playerch1);
		turn_camera(&st.playerch2);

		SDL_FillRect(winsurf, NULL, 0);
		show_all(NULL, 0, st.ellipsoids, nplayers, &st.playerch1.cam);
		show_all(NULL, 0, st.ellipsoids, nplayers, &st.playerch2.cam);
		show_button(&st.playerch1.prevbtn, winsurf);
		show_button(&st.playerch1.nextbtn, winsurf);
		show_button(&st.playerch2.prevbtn, winsurf);
		show_button(&st.playerch2.nextbtn, winsurf);

		SDL_UpdateWindowSurface(win);
		looptimer_wait(&lt);
	}
}
