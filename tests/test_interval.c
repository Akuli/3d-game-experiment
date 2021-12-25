#include <assert.h>
#include <stdbool.h>
#include "../src/interval.h"

/*
Quick and dirty python script for visualizing intervals (sudo apt install python3-tk):

import re
import tkinter


input_string = '''
    {289, 331, 5, true},
    {316, 365, 43, true},
    {306, 393, 0, false},
    {354, 415, 81, true},
    {234, 385, 2, false},
    {415, 494, 113, true},
    {528, 599, 115, true},

    {385, 415, 81, true},
    {385, 393, 0, false},
    {234, 385, 2, false},
    {415, 494, 113, true},
    {528, 599, 115, true},
'''

def get_color(id):
    if id % 2 == 1:
        return 'cyan'
    if id == 0:
        return 'white'
    if id == 2:
        return 'brown'
    return 'black'

intervals = [
    (int(start), int(end), info_string)
    for start, end, info_string in re.findall(
        r'\{(\d+), (\d+), ([^}]*)',
        re.sub(r'\s+', ' ', input_string)
    )
]
print(intervals)
assert len(intervals) == 7+5, len(intervals)


root = tkinter.Tk()
canvas = tkinter.Canvas(root)
canvas.pack(fill='both', expand=True)

SIZE = 20
x = 0

def get_id(info_string):
    return int(re.search(r'^(\d+),', info_string).group(1))

for start, end, info_string in intervals:
    canvas.create_text(x + SIZE/2, 200, text=info_string, anchor='w', angle=90)
    canvas.create_rectangle(x, start, x+SIZE, end, fill=get_color(get_id(info_string)))
    x += SIZE

root.geometry('300x800')
root.mainloop()
*/

static bool intervals_equal(struct Interval a, struct Interval b)
{
	return (a.start == b.start &&
			a.end == b.end &&
			a.id == b.id &&
			a.allowoverlap == b.allowoverlap);
}

static bool interval_arrays_equal(
	const struct Interval *a, int alen,
	const struct Interval *b, int blen)
{
	if (alen != blen)
		return false;

	for (int i = 0 ; i < alen; i++) {
		if (!intervals_equal(a[i], b[i]))
			return false;
	}
	return true;
}

void test_non_overlapping_bug(void)
{
	// taken from gdb output
	struct Interval in[] = {
		// { start, end, id, allowoverlap }
		{289, 331, 5, true},
		{316, 365, 43, true},
		{306, 393, 0, false},
		{354, 415, 81, true},
		{234, 385, 2, false},
		{415, 494, 113, true},
		{528, 599, 115, true},
	};

#define INLEN ( sizeof(in)/sizeof(in[0]) )
	struct Interval out[INTERVAL_NON_OVERLAPPING_MAX(INLEN)];
	int outlen = interval_non_overlapping(in, INLEN, out);
#undef INLEN

	struct Interval shouldB[] = {
		{385, 393, 0, false},
		{385, 415, 81, true},
		{234, 385, 2, false},
		{415, 494, 113, true},
		{528, 599, 115, true},
	};

	assert(interval_arrays_equal(
		out, outlen,
		shouldB, sizeof(shouldB)/sizeof(shouldB[0])
	));
}
