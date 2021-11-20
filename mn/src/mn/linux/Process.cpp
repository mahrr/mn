#include "mn/Process.h"

#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>

namespace mn
{
	Process
	process_id()
	{
		return Process{ (uint64_t)getpid() };
	}

	Process
	process_parent_id()
	{
		return Process{ (uint64_t)getppid() };
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
	process_memory_info(Process p)
	{
		Memory_Info res{-1, -1};

		rusage r;
		getrusage(RUSAGE_SELF, &r);
		res.peak_memory_usage_in_bytes = (size_t)r.ru_maxrss * 1024ULL;

		int64_t rss = 0;
		if (auto f = fopen("/proc/self/statm", "r"))
		{
			mn_defer(fclose(f));

			if (fscanf(f, "%*s%lld", &rss) == 1)
			{
				res.current_memory_usage_in_bytes = rss * (int64_t)sysconf(_SC_PAGESIZE);
			}
		}
		return res;
	}
}