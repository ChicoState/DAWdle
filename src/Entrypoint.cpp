#include "DrillLib.h"
#include "Win32.h"
#include "DAWdle.h"


int main() {
	if (!drill_lib_init()) {
		return EXIT_FAILURE;
	}
	U32 result = DAWdle::run_dawdle();
	return result;
}