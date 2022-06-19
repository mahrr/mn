#include "mn/memory/CLib.h"
#include "mn/Context.h"
#include "mn/OS.h"

#include <stdlib.h>

namespace mn::memory
{
	Block
	CLib::alloc(size_t size, uint8_t)
	{
		if (size == 0)
			return {};

		Block res{};
		res.ptr = ::malloc(size);
		if (res.ptr == nullptr && size > 0)
			mn::panic("system out of memory");
		res.size = size;
		_memory_profile_alloc(res.ptr, res.size);
		return res;
	}

	void
	CLib::free(Block block)
	{
		_memory_profile_free(block.ptr, block.size);
		::free(block.ptr);
	}

	CLib*
	clib()
	{
		static CLib _clib_allocator;
		return &_clib_allocator;
	}
}
