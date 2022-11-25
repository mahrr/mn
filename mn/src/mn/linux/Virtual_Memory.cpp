#include "mn/Virtual_Memory.h"

#include <sys/mman.h>

namespace mn
{
	Block
	virtual_alloc(void* address_hint, size_t size)
	{
		Block result{};
		result.ptr = mmap(address_hint, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		if(result.ptr)
			result.size = size;
		return result;
	}

	void
	virtual_commit(Block block)
	{
		[[maybe_unused]] auto res = mprotect(block.ptr, block.size, PROT_READ | PROT_WRITE);
		mn_assert(res == 0);
	}

	void
	virtual_release(Block block)
	{
		[[maybe_unused]] auto res = mprotect(block.ptr, block.size, PROT_NONE);
		mn_assert(res == 0);
	}

	void
	virtual_free(Block block)
	{
		munmap(block.ptr, block.size);
	}
}
