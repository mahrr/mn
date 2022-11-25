#pragma once

#include "mn/Exports.h"
#include "mn/memory/Interface.h"
#include "mn/Base.h"

#include <stdint.h>
#include <stddef.h>

namespace mn::memory
{
	// virtual memory allocator which allocates memory directly from the OS's virtual table
	struct Virtual : Interface
	{
		~Virtual() = default;

		// allocates and commits a new memory block with the given size and alignment
		MN_EXPORT Block
		alloc(size_t size, uint8_t alignment) override;

		// commits the given block to physical memory
		MN_EXPORT void
		commit(Block block) override;

		// releases the given block from physical memory
		MN_EXPORT void
		release(Block block) override;

		// frees the given memory block, if the block is empty it does nothing
		MN_EXPORT void
		free(Block block) override;
	};

	// returns the global virtual memory allocator instance
	MN_EXPORT Virtual*
	virtual_mem();
}
