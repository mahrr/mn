#pragma once

#include "mn/Exports.h"
#include "mn/memory/Interface.h"
#include "mn/Base.h"
#include "mn/Str.h"
#include "mn/Thread.h"

#include <stdint.h>
#include <stddef.h>

namespace mn::memory
{
	// a full leak detector with call stack traces, which tracks allocations and their locations. if the program exists
	// without freeing a block of memory it will report the leak to stderr along with the allocation location in terms
	// of call stack and filenames and lines of each call
	struct Leak: Interface
	{
		constexpr static inline int CALLSTACK_MAX_FRAMES = 20;
		struct Node
		{
			size_t size;
			void* callstack[CALLSTACK_MAX_FRAMES];
			Node* next;
			Node* prev;
		};

		Node* head;
		Mutex mtx;
		bool report_on_destruct;

		// creates a new instance of the leak detector allocator
		MN_EXPORT
		Leak();

		// destroys the given leak allocator
		MN_EXPORT
		~Leak() override;

		// allocates a new memory block with the given size and alignment and tracks it
		MN_EXPORT Block
		alloc(size_t size, uint8_t alignment) override;

		// passes the call down to underlying allocator
		MN_EXPORT void
		commit(Block block) override;

		// passes the call down to underlying allocator
		MN_EXPORT void
		release(Block block) override;

		// frees the given block of memory and untracks it, if the block is empty it does nothing
		MN_EXPORT void
		free(Block block) override;

		// prints the memory leak report, it's useful in case you want to report alive memory in a custom point before
		// program exit, and you can indicate to it that you don't want it to report memory leaks on program exit by
		// setting the report_on_destruct boolean to false, if you set it to true it will still report memory leaks
		// on program exit
		MN_EXPORT void
		report(bool report_on_destruct);
	};

	// returns the global instance of memory leak detector
	MN_EXPORT Leak*
	leak();
}
