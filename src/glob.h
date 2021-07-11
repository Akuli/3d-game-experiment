// my dumb implementation of posix <glob.h>

// Example, without /**/ comments because "foo/*.txt" contains "/*":

//		glob_t gl = {0};
//		int ret = glob("foo/*.txt", 0, NULL, &gl);
//		if (ret != 0 && ret != GLOB_NOMATCH)
//			goto error;
//
//		ret = glob("bar/*.txt", GLOB_APPEND, NULL, &gl);
//		if (ret != 0 && ret != GLOB_NOMATCH)
//			goto error;
//
//		for (int i = 0; i < gl.gl_pathc; i++)
//			printf("%s\n", gl.gl_pathv[i]);
//		globfree(&gl);
//		return;
//
//	error:
//		printf("error: %s\n", strerror(errno));
//		globfree(&gl);

/*
Caveats:
- People on stackoverflow seem to use "*.*" on Windows when they want everything
  to match on Windows. That doesn't make much sense to me, because not all
  filenames have an extension. I don't know how "*.*" could possibly match a file
  name with no extension. However, the windows code seems to work for somewhat
  simple globs like "fart*.wav".
- If you glob for "foo/bar/baz", only "baz" may contain glob characters like "*" on
  Windows, but POSIX globs also allow "*" in "foo" and "bar".
- Wide character file names are not supported on Windows.
- Windows paths are limited to MAX_PATH bytes, with the '\0' at the end included.
*/


#ifndef MY_GLOB_H
#define MY_GLOB_H

#ifndef _WIN32
#include <glob.h>
#else

#include <windows.h>    // TODO: get MAX_PATH without all of windows.h?

#define GLOB_NOSPACE 1
#define GLOB_NOMATCH 2
#define GLOB_ABORTED 3   // never used

#define GLOB_APPEND 0x01

typedef struct {
	size_t gl_pathc;
	char (*gl_pathv)[MAX_PATH];   // WIN32_FIND_DATAA uses MAX_PATH
	size_t alloc;     // how many items fit to gl_pathv
} glob_t;

// errfunc must be NULL
int glob(const char *pat, int flags, int (*errfunc)(const char *, int), glob_t *pglob);

void globfree(glob_t *pglob);

#endif    // _WIN32
#endif    // MY_GLOB_H
