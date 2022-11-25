#include "mn/memory/Virtual.h"
#include "mn/Virtual_Memory.h"
#include "mn/Context.h"

namespace mn::memory
{
	Block
	Virtual::alloc(size_t size, uint8_t)
	{
		Block res = virtual_alloc(nullptr, size);
		_memory_profile_alloc(res.ptr, res.size);
		return res;
	}

	void
	Virtual::commit(Block block)
	{
		virtual_commit(block);
	}

	void
	Virtual::release(Block block)
	{
		virtual_release(block);
	}

	void
	Virtual::free(Block block)
	{
		_memory_profile_free(block.ptr, block.size);
		virtual_free(block);
	}

	Virtual*
	virtual_mem()
	{
		static Virtual _virtual_allocator;
		return &_virtual_allocator;
	}
}
