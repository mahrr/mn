#pragma once

#include "mn/Exports.h"
#include "mn/memory/Interface.h"
#include "mn/memory/CLib.h"
#include "mn/Base.h"

#include <stdint.h>
#include <stddef.h>

namespace mn::memory
{
	// a stack like memory allocator, which allocates and frees memory from its top
	struct Stack : Interface
	{
		Interface* meta;
		Block memory;
		uint8_t* alloc_head;
		size_t allocations_count;

		// creates a new stack allocator instance with the given size in bytes and the meta allocator (defaults to clib)
		MN_EXPORT
		Stack(size_t stack_size, Interface* meta = clib());

		// frees the given allocator
		MN_EXPORT
		~Stack() override;

		// allocates a new memory block with the given size and alignment
		MN_EXPORT Block
		alloc(size_t size, uint8_t alignment) override;

		// passes the call down to underlying allocator
		MN_EXPORT void
		commit(Block block) override;

		// passes the call down to underlying allocator
		MN_EXPORT void
		release(Block block) override;

		// frees the given block if and only if it's the most recently allocated block (top of stack), if the block
		// is empty it does nothing
		MN_EXPORT void
		free(Block block) override;

		// resets the entire stack back to its initial state, thus freeing the entire memory
		MN_EXPORT void
		free_all();
	};
}
