#include "mn/Process.h"

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

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
	process_memory_info(Process p)
	{
		Memory_Info res{-1, -1};
		// TODO: implement later
		return res;
	}
}
