#pragma once

#include "mn/Exports.h"
#include "mn/Stream.h"

namespace mn
{
	// captures the top frames_count callstack frames and writes it
	// to the given frames pointer, it returns capture frames count
	MN_EXPORT size_t
	callstack_capture(void** frames, size_t frames_count);

	// prints the captured callstack to the given stream
	MN_EXPORT void
	callstack_print_to(void** frames, size_t frames_count, Stream out);
}
