#pragma once

#include "mn/Exports.h"
#include "mn/Base.h"
#include "mn/memory/Interface.h"
#include "mn/memory/Stack.h"
#include "mn/memory/Arena.h"
#include "mn/memory/Buddy.h"
#include "mn/Context.h"

#include <stdint.h>
#include <utility>
#include <new>
#include <string.h>

namespace mn
{
	// a memory allocator handle
	using Allocator = memory::Interface*;


	// allocates from the given allocator the given size of memory with the specified alignment
	inline static Block
	alloc_from(Allocator self, size_t size, uint8_t alignment)
	{
		auto block = self->alloc(size, alignment);
		self->commit(block);
		return block;
	}

	// frees a block using the given allocator
	inline static void
	free_from(Allocator self, Block block)
	{
		self->release(block);
		self->free(block);
	}


	// allocates from the given allocator a single instance of the given type
	template<typename T>
	inline static T*
	alloc_from(Allocator self)
	{
		return (T*)alloc_from(self, sizeof(T), alignof(T)).ptr;
	}

	// frees from the given allocator a single instance of the given type
	template<typename T>
	inline static void
	free_from(Allocator self, const T* ptr)
	{
		return free_from(self, Block{ (void*)ptr, sizeof(T) });
	}


	// allocates from the top/default allocator the given size of memory with the specified alignment
	inline static Block
	alloc(size_t size, uint8_t alignment)
	{
		return alloc_from(allocator_top(), size, alignment);
	}

	// frees a block from the top/default allocator
	inline static void
	free(Block block)
	{
		return free_from(allocator_top(), block);
	}


	// allocates a single instance of type t from the top/default allocator
	template<typename T>
	inline static T*
	alloc()
	{
		return (T*)alloc(sizeof(T), alignof(T)).ptr;
	}

	// frees a single instance of type t from the top/default allocator
	template<typename T>
	inline static void
	free(const T* ptr)
	{
		return free(Block{ (void*)ptr, sizeof(T) });
	}


	// allocates a single instance of the given type and calls its constructor with the given arguments
	template<typename T, typename ... TArgs>
	inline static T*
	alloc_construct_from(Allocator self, TArgs&& ... args)
	{
		T* res = alloc_from<T>(self);
		::new (res) T(std::forward<TArgs>(args)...);
		return res;
	}

	// destructs and frees the given instance from the allocator
	template<typename T>
	inline static void
	free_destruct_from(Allocator self, T* ptr)
	{
		ptr->~T();
		free_from(self, ptr);
	}


	// allocates a single instance of the given type and calls its constructor with the given arguments
	template<typename T, typename ... TArgs>
	inline static T*
	alloc_construct(TArgs&& ... args)
	{
		T* res = alloc<T>();
		::new (res) T(std::forward<TArgs>(args)...);
		return res;
	}

	// destructs and frees the given instance from the top/default allocator
	template<typename T>
	inline static void
	free_destruct(T* ptr)
	{
		ptr->~T();
		free(ptr);
	}


	// allocates a single instance of the given type and zeros the memory
	template<typename T>
	inline static T*
	alloc_zerod_from(Allocator self)
	{
		Block block = alloc_from(self, sizeof(T), alignof(T));
		block_zero(block);
		return (T*)block.ptr;
	}

	// allocates a single instance of the given type and zeros the memory
	template<typename T>
	inline static T*
	alloc_zerod()
	{
		Block block = alloc(sizeof(T), alignof(T));
		block_zero(block);
		return (T*)block.ptr;
	}

	// creates a new stack allocator with the given size and using the meta allocator
	// read more about stack allocator in Stack.h
	inline static memory::Stack*
	allocator_stack_new(size_t stack_size, Allocator meta = memory::clib())
	{
		return alloc_construct<memory::Stack>(stack_size, meta);
	}

	// creates a new arena allocator with the given block size and meta allocator
	// read more about arena allocator in Arena.h
	inline static memory::Arena*
	allocator_arena_new(size_t block_size = 4096, Allocator meta = memory::clib())
	{
		return alloc_construct<memory::Arena>(block_size, meta);
	}

	// creates a new buddy allocator with the given heap size and meta allocator
	// read more about buddy allocator in Buddy.h
	inline static memory::Buddy*
	allocator_buddy_new(size_t heap_size = 1ULL * 1024ULL * 1024ULL, Allocator meta = memory::virtual_mem())
	{
		return alloc_construct<memory::Buddy>(heap_size, meta);
	}

	// frees the given allocator
	inline static void
	allocator_free(Allocator self)
	{
		free_destruct(self);
	}

	// destruct overload for allocator free
	inline static void
	destruct(Allocator self)
	{
		allocator_free(self);
	}

	// copies the given block of bytes using the given allocator
	inline static Block
	block_clone(const Block& other, Allocator allocator = allocator_top())
	{
		if (block_is_empty(other))
			return Block{};

		Block self = alloc_from(allocator, other.size, alignof(int));
		::memcpy(self.ptr, other.ptr, other.size);
		return self;
	}
}
