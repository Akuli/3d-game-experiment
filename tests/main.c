#include <stdio.h>

int main(void)
{
#define RUN(Name) void Name(void); printf("Running %s\n", #Name); Name()
#include "../generated/run_tests.h"

	printf("ok\n");
	return 0;
}
