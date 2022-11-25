#include "mn/memory/Stack.h"
#include "mn/OS.h"
#include "mn/Assert.h"

namespace mn::memory
{
	Stack::Stack(size_t stack_size, Interface* meta)
	{
		mn_assert(stack_size != 0);
		this->meta = meta;
		this->memory = meta->alloc(stack_size, alignof(uint8_t));
		this->alloc_head = (uint8_t*)this->memory.ptr;
		this->allocations_count = 0;
	}

	Stack::~Stack()
	{
		this->meta->free(this->memory);
	}

	Block
	Stack::alloc(size_t size, uint8_t)
	{
		if (size == 0)
			return {};

		ptrdiff_t used_memory = this->alloc_head - (uint8_t*)this->memory.ptr;
		[[maybe_unused]] size_t free_memory = this->memory.size - used_memory;
		if (free_memory < size)
			panic("stack allocator out of memory");

		uint8_t* ptr = this->alloc_head;
		this->alloc_head = ptr + size;
		this->allocations_count++;
		return Block{ ptr, size };
	}

	void
	Stack::commit(Block block)
	{
		meta->commit(block);
	}

	void
	Stack::release(Block block)
	{
		meta->release(block);
	}

	void
	Stack::free(Block)
	{
		mn_assert(this->allocations_count > 0);
		--this->allocations_count;
		if (this->allocations_count == 0)
			this->alloc_head = (uint8_t*)this->memory.ptr;
	}

	void
	Stack::free_all()
	{
		this->allocations_count = 0;
		this->alloc_head = (uint8_t*)this->memory.ptr;
	}
}
