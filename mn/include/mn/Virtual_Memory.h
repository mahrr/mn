#pragma once

#include "mn/Exports.h"
#include "mn/Base.h"

namespace mn
{
	// allocates a block of memory using OS virtual memory
	MN_EXPORT Block
	virtual_alloc(void* address_hint, size_t size);

	// commits the block to physical memory
	MN_EXPORT void
	virtual_commit(Block block);

	// releases the block from physical memory
	MN_EXPORT void
	virtual_release(Block block);

	// frees a block from OS virtual memory
	MN_EXPORT void
	virtual_free(Block block);
}
