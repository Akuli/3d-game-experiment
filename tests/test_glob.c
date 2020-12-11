#include <assert.h>
#include <string.h>
#include "../src/glob.c"

void test_glob_success(void)
{
	glob_t gl;
	assert(glob("README.*", 0, NULL, &gl) == 0);
	assert(glob("assets/sounds/farts/*5.wav", GLOB_APPEND, NULL, &gl) == 0);

	assert(gl.gl_pathc == 2);
	assert(strcmp(gl.gl_pathv[0], "README.md") == 0);
	assert(strcmp(gl.gl_pathv[1], "assets/sounds/farts/fart5.wav") == 0);

	globfree(&gl);
}

void test_globfree_do_nothing(void)
{
	glob_t gl = {0};
	globfree(&gl);
}

void test_glob_nomatch(void)
{
	glob_t gl;
	assert(glob("foobarbizbaz.*", 0, NULL, &gl) == GLOB_NOMATCH);
	assert(glob("src/foobarbizbaz.*", 0, NULL, &gl) == GLOB_NOMATCH);
}
