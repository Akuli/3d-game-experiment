#ifdef _WIN32
	#include <direct.h>
	#include <windows.h>
	#include "misc.h"
#else
	// includes are from glob(3), mkdir(2) gethostname(2)
	#define _POSIX_C_SOURCE 200112L  // for gethostname
	#include <glob.h>
	#include <sys/stat.h>
	#include <sys/types.h>   // IWYU pragma: keep
	#include <unistd.h>
	// python uses 777 as default perms, see help(os.mkdir)
	#define _mkdir(path) mkdir((path), 0777)
#endif

#include "log.h"
#include <math.h>
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
	if (_mkdir("logs") == -1) {
		log_printf("mkdir error: %s", strerror(errno));
		// don't stop here, it could e.g. exist already
	}

	char fname[100] = {0};
	strftime(
		fname, sizeof(fname)-1,    // null-terminate even if strftime fails
		"logs/%Y-%m-%d-%a.txt",    // log filenames can be sorted alphabetically
		localtime((time_t[]){ time(NULL) })
	);

	// "b" instead of text mode to make sure that windows doesn't destroy utf8 characters
	if (( logfile = fopen(fname, "ab") ))
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

	// this doesn't print the utf8 correctly on windows, but windows cmd.exe is a joke anyway
	fprintf(stderr, "%s\n", msg);
	fflush(stderr);

	if (logfile) {
		// all strings are utf8 here, log file will be utf8
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

// useful for putting the game on usb stick, helps distinguish logs from different computers
static void log_computer_name(void)
{
#ifdef _WIN32
	wchar_t buf[1024] = {0};
	DWORD sz = sizeof(buf)/sizeof(buf[0]) - 1;
	if (GetComputerNameW(buf, &sz))
		log_printf("computer name: %s", misc_windows_to_utf8(buf));
	else
		log_printf("error when getting computer name: %s", strerror(errno));
#else
	char buf[1024] = {0};
	if (gethostname(buf, sizeof(buf)-1) == 0)
		log_printf("hostname: %s", buf);
	else
		log_printf("error when getting hostname: %s", strerror(errno));
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
	log_computer_name();
	remove_old_logfiles();
}
