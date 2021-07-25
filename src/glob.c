#include "glob.h"

#ifdef _WIN32

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <windows.h>
#include "misc.h"


typedef char PathBuf[MAX_PATH];

static int compare_pathbufs(const void *aptr, const void *bptr)
{
	return strcmp(*(const PathBuf *)aptr, *(const PathBuf *)bptr);
}

static PathBuf *get_next_glob_pointer(glob_t *pglob)
{
	assert(pglob->alloc >= pglob->gl_pathc);
	if (pglob->alloc == pglob->gl_pathc) {
		// it's full, need to allocate more
		size_t alloc = (pglob->gl_pathc == 0) ? 16 : (2*pglob->alloc);
		assert(alloc > pglob->gl_pathc);

		void *tmp = realloc(pglob->gl_pathv, sizeof(pglob->gl_pathv[0]) * alloc);
		if (!tmp)
			return NULL;

		pglob->gl_pathv = tmp;
		pglob->alloc = alloc;
	}

	static_assert(sizeof(pglob->gl_pathv[0]) == MAX_PATH, "");
	return &pglob->gl_pathv[pglob->gl_pathc++];
}

static const char *find_last_slash(const char *path)
{
	const char *bslash = strrchr(path, '\\');
	const char *fslash = strrchr(path, '/');

	// NULL < bla is ub: https://stackoverflow.com/a/35252004
	if (bslash && fslash) {
		if (bslash > fslash)
			return bslash;
		return fslash;
	}

	if (bslash) return bslash;
	if (fslash) return fslash;
	return NULL;
}

int glob(const char *pat, int flags, int (*errfunc)(const char *, int), glob_t *pglob)
{
	assert(errfunc == NULL);   // not implemented

	if (!(flags & GLOB_APPEND))
		*pglob = (glob_t){0};

	/*
	windows doesn't support wildcards in the directory, so we can find it by
	looking for last slash.
	*/
	const char *lastslash = find_last_slash(pat);

	WIN32_FIND_DATAW dat;
	HANDLE hnd = FindFirstFileW(misc_utf8_to_windows(pat), &dat);
	if (hnd == INVALID_HANDLE_VALUE)
		return GLOB_NOMATCH;

	size_t start = pglob->gl_pathc;
	do {
		PathBuf *buf = get_next_glob_pointer(pglob);
		if (!buf) {
			FindClose(hnd);
			if (!(flags & GLOB_APPEND))
				free(pglob->gl_pathv);
			return GLOB_NOSPACE;
		}

		if (lastslash == NULL)
			snprintf(*buf, sizeof(*buf), "%s", misc_windows_to_utf8(dat.cFileName));
		else
			snprintf(*buf, sizeof(*buf), "%.*s/%s", (int)(lastslash - pat), pat, misc_windows_to_utf8(dat.cFileName));
	} while (FindNextFileW(hnd, &dat));
	FindClose(hnd);

	// https://stackoverflow.com/q/29734737
	// Posix glob sorts by default, according to man page, but append still means append
	qsort(pglob->gl_pathv + start, pglob->gl_pathc - start, sizeof pglob->gl_pathv[0], compare_pathbufs);

	return 0;
}

void globfree(glob_t *pglob)
{
	free(pglob->gl_pathv);
}

#endif   // _WIN32
