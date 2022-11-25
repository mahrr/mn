#pragma once

#include "mn/Exports.h"
#include "mn/memory/Interface.h"
#include "mn/Base.h"

#include <stdint.h>
#include <stddef.h>

namespace mn::memory
{
	// a wrapper around system's libc allocator
	struct CLib : Interface
	{
		// uses malloc to allocate the given block
		MN_EXPORT Block
		alloc(size_t size, uint8_t alignment) override;

		// no-op for malloc
		MN_EXPORT void
		commit(Block block) override;

		// no-op for malloc
		MN_EXPORT void
		release(Block block) override;

		// frees the given block, if the block is empty it does nothing
		MN_EXPORT void
		free(Block block) override;
	};

	// returns the global instance of the libc allocator
	MN_EXPORT CLib*
	clib();
}
