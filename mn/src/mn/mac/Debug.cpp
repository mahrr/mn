#include "mn/Debug.h"
#include "mn/IO.h"

#include <cxxabi.h>
#if MN_BACKTRACE
#include <execinfo.h>
#endif

#include <stdlib.h>

namespace mn
{
	size_t
	callstack_capture([[maybe_unused]] void** frames, [[maybe_unused]] size_t frames_count)
	{
		#if MN_BACKTRACE
		::memset(frames, 0, frames_count * sizeof(frames));
		return backtrace(frames, frames_count);
		#else
		return 0;
		#endif
	}

	void
	callstack_print_to([[maybe_unused]] void** frames, [[maybe_unused]] size_t frames_count, [[maybe_unused]] Stream out)
	{
#ifndef NDEBUG
		char** symbols = backtrace_symbols(frames, frames_count);
		if(symbols)
		{
			for(size_t i = 0; i < frames_count; ++i)
			{
				// example output
				// 0   <module_name>     0x0000000000000000 function_name + 00
				char function_name[1024] = {};
				char address[48] = {};
				char module_name[1024] = {};
				int offset = 0;

				::sscanf(symbols[i], "%*s %s %s %s %*s %d", module_name, address, function_name, &offset);

				int status = 0;
				char* demangled_name = abi::__cxa_demangle(function_name, NULL, 0, &status);

				if(status == 0)
					print_to(out, "[{}]: {}\n", frames_count - i - 1, demangled_name);
				else
					print_to(out, "[{}]: {}\n", frames_count - i - 1, function_name);

				::free(demangled_name);
			}
			::free(symbols);
		}
#endif
	}
}
