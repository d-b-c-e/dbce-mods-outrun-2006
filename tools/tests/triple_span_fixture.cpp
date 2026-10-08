// Offline topology fixture for src/triple_span.hpp ([Triple] Screens = Separate monitors). Pure logic, no game.
#include "../../src/triple_span.hpp"
#include <cstdio>

static int failures;
static void check(bool ok, const char* what)
{
	std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		failures++;
}
static RECT R(long l, long t, long r, long b) { return RECT{ l, t, r, b }; }

int main()
{
	RECT out{};
	bool ok = TripleSpanFromMonitors({ R(0, 0, 2560, 1440), R(-2560, 0, 0, 1440), R(2560, 0, 5120, 1440) }, out);
	check(ok && out.left == -2560 && out.top == 0 && out.right == 5120 && out.bottom == 1440,
		"three contiguous equal monitors (primary in the middle, any order) span -2560..5120 x 0..1440");
	ok = TripleSpanFromMonitors({ R(0, 0, 1920, 1080), R(1920, 0, 3840, 1080), R(3840, 0, 5760, 1080) }, out);
	check(ok && out.left == 0 && out.right == 5760 && out.bottom == 1080, "three contiguous equal 1080p monitors from 0");
	check(!TripleSpanFromMonitors({ R(0, 0, 7680, 1440) }, out), "one display (Surround) is rejected");
	check(!TripleSpanFromMonitors({ R(0, 0, 2560, 1440), R(2560, 0, 5120, 1440) }, out), "two monitors are rejected");
	check(!TripleSpanFromMonitors({ R(-2560, 0, 0, 1440), R(0, 0, 2560, 1440), R(2560, 0, 5120, 1440), R(5120, 0, 7680, 1440) }, out),
		"four monitors are rejected");
	check(!TripleSpanFromMonitors({ R(-2570, 0, -10, 1440), R(0, 0, 2560, 1440), R(2560, 0, 5120, 1440) }, out),
		"a gap between monitors is rejected");
	check(!TripleSpanFromMonitors({ R(-2550, 0, 10, 1440), R(0, 0, 2560, 1440), R(2560, 0, 5120, 1440) }, out),
		"overlapping monitors are rejected");
	check(!TripleSpanFromMonitors({ R(-2560, 0, 0, 1080), R(0, 0, 2560, 1440), R(2560, 0, 5120, 1440) }, out),
		"unequal heights are rejected");
	check(!TripleSpanFromMonitors({ R(-1920, 0, 0, 1440), R(0, 0, 2560, 1440), R(2560, 0, 5120, 1440) }, out),
		"unequal widths are rejected");
	check(!TripleSpanFromMonitors({ R(-2560, 100, 0, 1540), R(0, 0, 2560, 1440), R(2560, 0, 5120, 1440) }, out),
		"a vertically offset monitor is rejected");
	check(!TripleSpanFromMonitors({ R(0, 0, 2560, 1440), R(0, 1440, 2560, 2880), R(0, 2880, 2560, 4320) }, out),
		"three stacked monitors are rejected");
	check(!TripleSpanFromMonitors({ R(0, 0, 0, 1440), R(0, 0, 0, 1440), R(0, 0, 0, 1440) }, out), "empty rectangles are rejected");
	std::printf("%s triple span topology, %d failures\n", failures ? "FAIL" : "ok", failures);
	return failures ? 1 : 0;
}
