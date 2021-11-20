#include "mn/Process.h"

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <mach/mach.h>

namespace mn
{
	Process
	process_id()
	{
		return Process{ static_cast<uint64_t>(getpid()) };
	}

	Process
	process_parent_id()
	{
		return Process{ static_cast<uint64_t>(getppid()) };
	}

	bool
	process_kill(Process p)
	{
		return kill(p.id, SIGTERM) == 0;
	}

	bool
	process_alive(Process p)
	{
		return kill(p.id, 0) == 0;
	}

	Memory_Info
	process_memory_info()
	{
		Memory_Info res{-1, -1};

		rusage r;
		getrusage(RUSAGE_SELF, &r);
		res.peak_memory_usage_in_bytes = (size_t)r.ru_maxrss;

		mach_task_basic_info info;
		mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
		if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS)
			res.current_memory_usage_in_bytes = (int64_t)info.resident_size;
		return res;
	}
}
