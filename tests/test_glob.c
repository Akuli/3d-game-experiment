#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/glob.h"
#include "../src/misc.h"

void test_glob_success(void)
{
	my_mkdir("generated");
	my_mkdir("generated/testdata");
	my_mkdir("generated/testdata/subdir");

	fclose(fopen("generated/testdata/x.py", "w"));
	fclose(fopen("generated/testdata/a.txt", "w"));
	fclose(fopen("generated/testdata/c.txt", "w"));  // Messy order to ensure it gets sorted
	fclose(fopen("generated/testdata/b.txt", "w"));
	fclose(fopen("generated/testdata/subdir/lol", "w"));

	glob_t gl;
	assert(glob("generated/testdata/subdir/*", 0, NULL, &gl) == 0);
	assert(glob("generated/testdata/*", GLOB_APPEND, NULL, &gl) == 0);
	assert(glob("generated/testdata/*.txt", GLOB_APPEND, NULL, &gl) == 0);

	const char *exp[] = {
		// testdata/subdir/*
		"generated/testdata/subdir/lol",
		// testdata/*
		"generated/testdata/a.txt",
		"generated/testdata/b.txt",
		"generated/testdata/c.txt",
		"generated/testdata/subdir",
		"generated/testdata/x.py",
		// testdata/*.txt
		"generated/testdata/a.txt",
		"generated/testdata/b.txt",
		"generated/testdata/c.txt",
	};
	assert(sizeof(exp)/sizeof(exp[0]) == gl.gl_pathc);
	for (int i = 0; i < gl.gl_pathc; i++)
		assert(strcmp(exp[i], gl.gl_pathv[i]) == 0);

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
