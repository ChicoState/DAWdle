#include "DrillLib.h"
#include "Win32.h"
#include "DAWdle.h"


extern "C" void __stdcall mainCRTStartup() {
	if (!drill_lib_init()) {
		ExitProcess(EXIT_FAILURE);
	}
	U32 result = DAWdle::run_dawdle();
	ExitProcess(result);
}