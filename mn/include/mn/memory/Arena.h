#pragma once

#include "mn/Exports.h"
#include "mn/memory/Interface.h"
#include "mn/memory/CLib.h"
#include "mn/Base.h"

#include <stdint.h>
#include <stddef.h>

namespace mn::memory
{
	// arena is a type of allocator which amortizes the cost of allocating alot of small size elements
	// because it allocates in bulk (for example 4Kb) and subdivide it on demand
	// we noticed that most of the time we free the entire arena at once, that's why arena doesn't free
	// individual elements, which simplifies internal book keeping a lot, also this is symmetric to
	// the way it does allocation. in short arena is a bulk allocator, it allocates in bulk and frees in bulk
	struct Arena : Interface
	{
		struct Node
		{
			Block mem;
			uint8_t* alloc_head;
			Node* next;
		};

		struct State
		{
			Node* head;
			uint8_t* alloc_head;
			size_t total_mem;
			size_t used_mem;
			size_t highwater_mem;
		};

		Interface* meta;
		Node* head;
		// contains the block size in bytes, this is the granularity of allocation/free
		size_t block_size;
		// total amount of memory used in bytes, including fragmentation and other wasted memory
		size_t total_mem;
		// actual used memory in bytes
		size_t used_mem;
		// peak memory usage in bytes
		size_t highwater_mem;
		// determines the threshold amount of temporary memory between the current and previous highwater
		// that will trigger a readjust (free/realloc), default value is 4MB
		size_t clear_all_readjust_threshold;
		size_t clear_all_current_highwater;
		size_t clear_all_previous_highwater;

		// creates a new arena allocator with the given block size (in bytes), and the meta allocator (defaults to system malloc)
		MN_EXPORT
		Arena(size_t block_size, Interface* meta = clib());

		// frees the given arena
		MN_EXPORT
		~Arena() override;

		// allocates a block with the given size and alignement
		MN_EXPORT Block
		alloc(size_t size, uint8_t alignment) override;

		// passes the call down to underlying allocator
		MN_EXPORT void
		commit(Block block) override;

		// passes the call down to underlying allocator
		MN_EXPORT void
		release(Block block) override;

		// does nothing, arena doesn't support individual frees
		MN_EXPORT void
		free(Block block) override;

		// reserves the given amount of memory
		MN_EXPORT void
		grow(size_t size);

		// frees the entire arena to the meta allocator
		MN_EXPORT void
		free_all();

		// resets the allocation state back but doesn't free the memory to the meta allocator, which is useful for memory reuse
		MN_EXPORT void
		clear_all();

		// checks whether this arena owns this pointer, which is useful for debugging and various assertions
		MN_EXPORT bool
		owns(void* ptr) const;

		MN_EXPORT State
		checkpoint() const;

		MN_EXPORT void
		restore(State state);
	};
}

namespace mn
{
	// frees the entire arena back to the meta allocator
	inline static void
	allocator_arena_free_all(memory::Arena* self)
	{
		self->free_all();
	}

	// resets the allocation state back but doesn't free the memory to the meta allocator, which is useful for memory reuse
	inline static void
	allocator_arena_clear_all(memory::Arena* self)
	{
		self->clear_all();
	}

	// checks whether this arena owns this pointer, which is useful for debugging and various assertions
	inline static bool
	allocator_arena_owns(const memory::Arena* self, void* ptr)
	{
		return self->owns(ptr);
	}

	// saves the state of arena allocator to be used in a restore function later
	inline static memory::Arena::State
	allocator_arena_checkpoint(const memory::Arena* self)
	{
		return self->checkpoint();
	}

	// restores the arena back to the saved checkpoint
	inline static void
	allocator_arena_restore(memory::Arena* self, memory::Arena::State state)
	{
		self->restore(state);
	}
}
