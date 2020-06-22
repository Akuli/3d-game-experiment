#ifdef _WIN32
	#include <direct.h>
	#include <windows.h>
	#define my_mkdir(path) _mkdir((path))
#else
	#include <glob.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	// python uses 777 as default perms, see help(os.mkdir)
	#define my_mkdir(path) mkdir((path), 0777)
#endif

#include "log.h"
#include "math.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>

// must be global because there's no other way to pass data to atexit callbacks
static FILE *logfile = NULL;

static void close_log_file(void) { fclose(logfile); }
static void open_log_file(void)
{
	if (my_mkdir("logs") == -1) {
		log_printf("mkdir error: %s", strerror(errno));
		// don't stop here, it could e.g. exist already
	}

	char fname[100] = {0};
	strftime(
		fname, sizeof(fname)-1,    // null-terminate even if strftime fails
		"logs/%Y-%m-%d-%a.txt",    // log filenames can be sorted alphabetically
		localtime((time_t[]){ time(NULL) })
	);

	if (( logfile = fopen(fname, "a") ))
		atexit(close_log_file);
	else
		log_printf("opening log file failed: %s", strerror(errno));
}

static void logging_callback_for_sdl(void *userdata, int categ, SDL_LogPriority prio, const char *msg)
{
	// silence unused variable warnings
	(void)userdata;
	(void)categ;
	(void)prio;

	time_t t = time(NULL);
	const char *tstr = ctime(&t);

	int tlen = strlen(tstr);
	if (tlen > 0 && tstr[tlen-1] == '\n')
		tlen--;

	fprintf(stderr, "%s\n", msg);
	fflush(stderr);

	if (logfile) {
		fprintf(logfile, "[%.*s] %s\n", tlen, tstr, msg);
		fflush(logfile);
	}
}

#define SECOND 1
#define MINUTE (60*SECOND)
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)

static void remove_old_logfile(const char *path)
{
	// windows doesn't have strptime()
	int year=0, month=0, day=0;
	if (sscanf(path, "logs/%d-%d-%d", &year, &month, &day) < 3) {
		log_printf("unexpected log file path '%s'", path);
		return;
	}

	double age = difftime(time(NULL), mktime(&(struct tm){
		.tm_year = year-1900,   // yes, this is how it works
		.tm_mon = month-1,
		.tm_mday = day,
	}));

	if (age < 0) {
		log_printf("creation of '%s' seems to be %f days in the future", path, fabs(age)/DAY);
	} else if (age > 7*DAY) {
		log_printf("removing '%s' (%f days old)", path, age/DAY);
		if (remove(path) < 0)
			log_printf("removing failed: %s", strerror(errno));
	} else {
		log_printf("not removing '%s' yet (%f days old)", path, age/DAY);
	}
}

static void remove_old_logfiles(void)
{
#ifdef _WIN32
	WIN32_FIND_DATAA fdata;
	HANDLE h = FindFirstFileA("logs\\*.txt", &fdata);
	if (h == INVALID_HANDLE_VALUE) {
		log_printf("error while listing log files: %s", strerror(errno));
		return;
	}

	do {
		char path[MAX_PATH];
		snprintf(path, sizeof path, "logs/%s", fdata.cFileName);   // must be forward slash for sscanf()
		remove_old_logfile(path);
	} while (FindNextFileA(h, &fdata));
	FindClose(h);

#else
	glob_t gl;
	int r = glob("logs/*.txt", 0, NULL, &gl);
	switch(r) {
		case 0: break;
		case GLOB_NOSPACE: log_printf("glob ran out of memory"); return;
		case GLOB_ABORTED: log_printf("glob error: %s", strerror(errno)); return;
		case GLOB_NOMATCH: log_printf("no log files found"); return;
		default:           log_printf("got unexpected return value from glob(): %d", r); return;
	}

	for (int i = 0; i < gl.gl_pathc; i++)
		remove_old_logfile(gl.gl_pathv[i]);
	globfree(&gl);
#endif
}

void log_init(void)
{
	/*
	Make sure that log_printf() will work while running open_log_file(), even
	though it won't print to the log file (stderr only)
	*/
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
	SDL_LogSetOutputFunction(logging_callback_for_sdl, NULL);

	open_log_file();
	log_printf("------------------------------");
	log_printf("game is starting");
	log_printf("------------------------------");
	remove_old_logfiles();
}
