#include "common.h"
#include <assert.h>

int iclamp(int val, int min, int max)
{
	assert(min <= max);
	if (val < min)
		return min;
	if (val > max)
		return max;
	return val;
}
