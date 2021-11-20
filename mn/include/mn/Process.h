#pragma once

#include "mn/Exports.h"

#include <stdint.h>

namespace mn
{
	// a process id
	struct Process
	{
		uint64_t id;
	};

	// returns the current process id
	MN_EXPORT Process
	process_id();

	// returns the parent id of this process, if it has no parent it will
	// return a zero handle Process{}
	MN_EXPORT Process
	process_parent_id();

	// tries to kill the given process and returns whether it was successful
	MN_EXPORT bool
	process_kill(Process p);

	// returns whether the given process is alive
	MN_EXPORT bool
	process_alive(Process p);

	struct Memory_Info
	{
		int64_t peak_memory_usage_in_bytes;
		int64_t current_memory_usage_in_bytes;
	};

	// returns the memory usage info of the given process
	MN_EXPORT Memory_Info
	process_memory_info(Process p);
}
