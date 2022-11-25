#pragma once

#include "mn/Exports.h"
#include "mn/memory/Interface.h"
#include "mn/Base.h"
#include "mn/Str.h"
#include "mn/Thread.h"

#include <atomic>
#include <stdint.h>

namespace mn::memory
{
	// a simple leak detector allocator, which keeps track of the count of allocations and makes sure you free them
	// before the program exists, if the program exists with memory allocations still alive it'll report the count
	// of allocations that's still alive in stderr, this is mainly used automatically in debug builds which helps
	// with memory leak detection
	struct Fast_Leak: Interface
	{
		std::atomic<size_t> atomic_size;
		std::atomic<size_t> atomic_count;

		// creates a new instance of the fast leak allocator
		MN_EXPORT
		Fast_Leak();

		// destroys the given instance of fast leak allocator
		MN_EXPORT
		~Fast_Leak() override;

		// allocates a new memory block with the given size and alignment, and tracks it
		MN_EXPORT Block
		alloc(size_t size, uint8_t alignment) override;

		// passes the call down to underlying allocator
		MN_EXPORT void
		commit(Block block) override;

		// passes the call down to underlying allocator
		MN_EXPORT void
		release(Block block) override;

		// frees the given block of memory, and untracks it, if the block is empty it does nothing
		MN_EXPORT void
		free(Block block) override;
	};

	// returns the global instance of the fast leak allocator
	MN_EXPORT Fast_Leak*
	fast_leak();
}
