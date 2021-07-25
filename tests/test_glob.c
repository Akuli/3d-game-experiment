#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/glob.h"
#include "../src/misc.h"

void test_glob_success(void)
{
	// TODO: these generate warnings when dirs exist
	misc_mkdir("generated");
	misc_mkdir("generated/testdata");

	fclose(fopen("generated/testdata/a", "w"));
	fclose(fopen("generated/testdata/c", "w"));
	fclose(fopen("generated/testdata/b", "w"));

	glob_t gl;
	assert(glob("README.*", 0, NULL, &gl) == 0);
	assert(glob("generated/testdata/*", GLOB_APPEND, NULL, &gl) == 0);
	assert(glob("assets/sounds/farts/*5.wav", GLOB_APPEND, NULL, &gl) == 0);

	for (int i = 0; i < gl.gl_pathc; i++)
		printf("asd %s\n", gl.gl_pathv[i]);

	assert(gl.gl_pathc == 5);
	assert(strcmp(gl.gl_pathv[0], "README.md") == 0);
	assert(strcmp(gl.gl_pathv[1], "generated/testdata/a") == 0);
	assert(strcmp(gl.gl_pathv[2], "generated/testdata/b") == 0);
	assert(strcmp(gl.gl_pathv[3], "generated/testdata/c") == 0);
	assert(strcmp(gl.gl_pathv[4], "assets/sounds/farts/fart5.wav") == 0);

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
