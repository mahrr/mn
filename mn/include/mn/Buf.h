#pragma once

#include "mn/Base.h"
#include "mn/Memory.h"
#include "mn/Assert.h"

#include <string.h>

#include <initializer_list>

namespace mn
{
	// Buf is the workhorse of the containers, it's a dynamic array
	template<typename T>
	struct Buf
	{
		// the allocator which this buf instance uses
		Allocator allocator;
		// pointer to the memory allocated by this buf
		T* ptr;
		// count of elements that exist in this buf
		size_t count;
		// capacity of elements which the allocated memroy can hold
		size_t cap;

		T&
		operator[](size_t ix)
		{
			mn_assert(ix < count);
			return ptr[ix];
		}

		const T&
		operator[](size_t ix) const
		{
			mn_assert(ix < count);
			return ptr[ix];
		}
	};

	// creates a new buf using the default allocator
	template<typename T>
	inline static Buf<T>
	buf_new()
	{
		Buf<T> self{};
		self.allocator = allocator_top();
		return self;
	}

	// creates a buf instance with a custom allocator
	template<typename T>
	inline static Buf<T>
	buf_with_allocator(Allocator allocator)
	{
		Buf<T> self{};
		self.allocator = allocator;
		return self;
	}

	// creates a buf and populates it with the given initializer list of element
	// example: auto array = mn::buf_lit({1, 2, 3});
	template<typename T>
	inline static Buf<T>
	buf_lit(const std::initializer_list<T>& values)
	{
		Buf<T> self = buf_new<T>();
		buf_reserve(self, values.size());
		for(const auto& val: values)
			self.ptr[self.count++] = val;
		return self;
	}

	// creates a new buf instance with the given count of elements, note: the memory is allocated and not initialized
	template<typename T>
	inline static Buf<T>
	buf_with_count(size_t count)
	{
		Buf<T> self = buf_new<T>();
		buf_resize(self, count);
		return self;
	}

	// creates a new buf instance with the given capcity of elements
	template<typename T>
	inline static Buf<T>
	buf_with_capacity(size_t cap)
	{
		Buf<T> self = buf_new<T>();
		buf_reserve(self, cap);
		return self;
	}

	// frees the given buf instance, if it's empty it does nothing
	template<typename T>
	inline static void
	buf_free(Buf<T>& self)
	{
		if(self.cap && self.allocator)
			free_from(self.allocator, Block{ self.ptr, self.cap * sizeof(T) });
		self.cap = 0;
		self.count = 0;
		self.ptr = nullptr;
	}

	// a general destruct function overload which calls the destructor of the given value
	template<typename T>
	inline static void
	destruct(T& value)
	{
		value.~T();
	}

	// default no-op destruct functions for the builtin types
	inline static void destruct(char&) {}
	inline static void destruct(uint8_t&) {}
	inline static void destruct(uint16_t&) {}
	inline static void destruct(uint32_t&) {}
	inline static void destruct(uint64_t&) {}
	inline static void destruct(int8_t&) {}
	inline static void destruct(int16_t&) {}
	inline static void destruct(int32_t&) {}
	inline static void destruct(int64_t&) {}
	inline static void destruct(float&) {}
	inline static void destruct(double&) {}
	inline static void destruct(long double&) {}

	// a custom overload for buf which loops over all the elements and calls destruct, this is useful for destructing
	// a big hierarchy without memory leaks, for example when you free mn::Buf<mn::Buf<int>> if you use mn::buf_free
	// you'll only free the top level buf and won't free the small mn::Buf<int> inside, it's more appropriate to use
	// destruct instead of mn::buf_free for this case which will destruct each mn::Buf<int> inside the bigger buf
	template<typename T>
	inline static void
	destruct(Buf<T>& self)
	{
		for(size_t i = 0; i < self.count; ++i)
			destruct(self[i]);
		buf_free(self);
	}

	// indexes the buf with signed integers like python
	// examples:
	// mn::buf_of(b, 0) = b[0]
	// mn::buf_of(b, 1) = b[1]
	// mn::buf_of(b, -1) = b[b.count - 1];
	template<typename T>
	inline static T&
	buf_of(Buf<T>& self, int ix)
	{
		if(ix < 0)
			ix = self.count + ix;
		mn_assert(ix >= 0);
		mn_assert(ix < self.count);
		return self.ptr[ix];
	}

	// indexes the buf with signed integers like python
	// examples:
	// mn::buf_of(b, 0) = b[0]
	// mn::buf_of(b, 1) = b[1]
	// mn::buf_of(b, -1) = b[b.count - 1];
	template<typename T>
	inline static const T&
	buf_of(const Buf<T>& self, int ix)
	{
		if(ix < 0)
			ix = self.count + ix;
		mn_assert(ix >= 0);
		mn_assert(ix < self.count);
		return self.ptr[ix];
	}

	// clears the given buf, by changing the element count to 0, it doesn't free the memory not does it destruct the
	// elements
	template<typename T>
	inline static void
	buf_clear(Buf<T>& self)
	{
		if (self.allocator)
			self.allocator->release(Block{self.ptr, self.cap * sizeof(T)});
		self.count = 0;
	}

	// fills the given buf with a value
	template<typename T, typename R>
	inline static void
	buf_fill(Buf<T>& self, const R& value)
	{
		for(size_t i = 0; i < self.count; ++i)
			self.ptr[i] = value;
	}

	template<typename T>
	inline static void
	_buf_reserve_exact(Buf<T>& self, size_t new_count)
	{
		if (self.allocator == nullptr)
			self.allocator = allocator_top();

		auto new_block = self.allocator->alloc(new_count * sizeof(T), alignof(T));

		if(self.count)
		{
			self.allocator->commit(Block{new_block.ptr, self.count * sizeof(T)});
			::memcpy(new_block.ptr, self.ptr, self.count * sizeof(T));
		}
		if(self.cap)
			free_from(self.allocator, Block{ self.ptr, self.cap * sizeof(T) });
		self.ptr = (T*)new_block.ptr;
		self.cap = new_count;
	}

	// ensures the given buf has the capacity to hold the added size, which might allocate memory in case the current
	// capacity can't hold the added size
	template<typename T>
	inline static void
	buf_reserve(Buf<T>& self, size_t added_size)
	{
		if(self.count + added_size <= self.cap)
			return;

		size_t next_cap = size_t(self.cap * 1.5f);
		size_t accurate_cap = self.count + added_size;
		size_t request_cap = next_cap > accurate_cap ? next_cap : accurate_cap;
		_buf_reserve_exact(self, request_cap);
	}

	// resizes the given buf to the new count of elements
	template<typename T>
	inline static void
	buf_resize(Buf<T>& self, size_t new_size)
	{
		if(new_size > self.count)
		{
			buf_reserve(self, new_size - self.count);
			self.allocator->commit(Block{self.ptr + self.count, (new_size - self.count) * sizeof(T)});
		}
		else
		{
			self.allocator->release(Block{self.ptr + new_size, (self.count - new_size) * sizeof(T)});
		}
		self.count = new_size;
	}

	// resizes the given buf to the new size and in case the new size is bigger than the current size it fills the newly
	// allocated elements with the given value
	template<typename T, typename U>
	inline static void
	buf_resize_fill(Buf<T>& self, size_t new_size, const U& fill_val)
	{
		auto old_count = self.count;
		buf_resize(self, new_size);
		for(size_t i = old_count; i < new_size; ++i)
			self.ptr[i] = fill_val;
	}

	// shrinks the given buf to exactly the size of the contained elements
	template<typename T>
	inline static void
	buf_shrink_to_fit(Buf<T>& self)
	{
		if(self.cap == self.count || self.count == 0)
			return;

		if (self.allocator == nullptr)
			self.allocator = allocator_top();

		Block new_block = alloc_from(self.allocator, self.count * sizeof(T), alignof(T));
		::memcpy(new_block.ptr, self.ptr, self.count * sizeof(T));
		free_from(self.allocator, Block { self.ptr, self.cap * sizeof(T) });
		self.ptr = (T*)new_block.ptr;
		self.cap = self.count;
	}

	// pushes a new value to the end of the given buf
	template<typename T, typename R>
	inline static T*
	buf_push(Buf<T>& self, const R& value)
	{
		if(self.count == self.cap)
			buf_reserve(self, self.cap ? self.cap * 2 : 8);

		self.allocator->commit(Block{self.ptr + self.count, sizeof(T)});
		self.ptr[self.count] = T(value);
		++self.count;
		return self.ptr + self.count - 1;
	}

	// pushes the given value n number of times (according to the given count) to the end of the given buf
	template<typename T>
	inline static void
	buf_pushn(Buf<T>& self, size_t count, const T& value)
	{
		size_t i = self.count;
		buf_resize(self, self.count + count);
		for(; i < self.count; ++i)
			self.ptr[i] = value;
	}

	// inserts a new value at a specific index
	template<typename T, typename R>
	inline static T*
	buf_insert(Buf<T>& self, size_t index, const R& value)
	{
		mn_assert(index <= self.count);
		if (index == self.count)
			return buf_push(self, value);
		if (self.count == self.cap)
			buf_reserve(self, self.cap ? self.cap * 2 : 8);

		self.allocator->commit(Block{self.ptr + self.count, sizeof(T)});
		::memmove(self.ptr + index + 1, self.ptr + index, (self.count - index) * sizeof(T));
		++self.count;
		self.ptr[index] = T(value);
		return self.ptr + index;
	}

	// remove the value found at the given index while preserving the order of the elements in the given buf
	template<typename T>
	inline static void
	buf_remove_ordered(Buf<T>& self, size_t index)
	{
		mn_assert(index < self.count);
		::memmove(self.ptr + index, self.ptr + index + 1, (self.count - index - 1) * sizeof(T));
		--self.count;
		self.allocator->release(Block{self.ptr + self.count, sizeof(T)});
	}

	// pushes a range of elements to the end of the given buf
	template<typename T>
	inline static void
	buf_concat(Buf<T>& self, const T* begin, const T* end)
	{
		size_t added_count = end - begin;
		size_t old_count = self.count;
		buf_resize(self, old_count + added_count);
		::memcpy(self.ptr + old_count, begin, added_count * sizeof(T));
	}

	// concatenates two bufs together
	template<typename T>
	inline static void
	buf_concat(Buf<T>& self, const Buf<T>& other)
	{
		buf_concat(self, other.ptr, other.ptr + other.count);
	}

	// pops the last element of the buf
	template<typename T>
	inline static void
	buf_pop(Buf<T>& self)
	{
		mn_assert(self.count > 0);
		--self.count;
		self.allocator->release(Block{self.ptr + self.count, sizeof(T)});
	}

	// rmeoves any element which the predicate signals (by returning true)
	template<typename T, typename TLambda>
	inline static void
	buf_remove_if(Buf<T>& self, TLambda&& pred)
	{
		T* begin_it = self.ptr;
		T* end_it = self.ptr + self.count;
		T* front_it = self.ptr;

		for(T* it = begin_it; it != end_it; ++it)
		{
			if(pred(*it) == false)
			{
				*front_it = *it;
				++front_it;
			}
		}

		auto removed_count = end_it - front_it;
		self.count -= removed_count;
		if (self.allocator)
			self.allocator->release(Block{self.ptr + self.count, removed_count * sizeof(T)});
	}

	// removes the element found at the given index (will not keep order)
	template<typename T>
	inline static void
	buf_remove(Buf<T>& self, size_t ix)
	{
		mn_assert(ix < self.count);
		if(ix + 1 != self.count)
		{
			T tmp = self.ptr[self.count - 1];
			self.ptr[self.count - 1] = self.ptr[ix];
			self.ptr[ix] = tmp;
		}
		--self.count;
		self.allocator->release(Block{self.ptr + self.count, sizeof(T)});
	}

	// removes the element which the given iterator points to (will not keep order)
	template<typename T>
	inline static void
	buf_remove(Buf<T>& self, T* it)
	{
		buf_remove(self, it - self.ptr);
	}

	// returns the top/last element in the given buf
	template<typename T>
	inline static const T&
	buf_top(const Buf<T>& self)
	{
		mn_assert(self.count > 0);
		return self.ptr[self.count - 1];
	}

	// returns the top/last element in the given buf
	template<typename T>
	inline static T&
	buf_top(Buf<T>& self)
	{
		mn_assert(self.count > 0);
		return self.ptr[self.count - 1];
	}

	// returns whether the given buf is empty or not
	template<typename T>
	inline static bool
	buf_empty(const Buf<T>& self)
	{
		return self.count == 0;
	}

	// returns an iterator to the start of the given buf
	template<typename T>
	inline static const T*
	buf_begin(const Buf<T>& self)
	{
		return self.ptr;
	}

	// returns an iterator to the start of the given buf
	template<typename T>
	inline static T*
	buf_begin(Buf<T>& self)
	{
		return self.ptr;
	}

	// returns an iterator at the end of the given buf
	template<typename T>
	inline static const T*
	buf_end(const Buf<T>& self)
	{
		return self.ptr + self.count;
	}

	// returns an iterator at the end of the given buf
	template<typename T>
	inline static T*
	buf_end(Buf<T>& self)
	{
		return self.ptr + self.count;
	}

	// begin function overload which is useful when using range for loop ex. `for (auto x: buf)` and various standard
	// algorithms ex. `std::sort(begin(buf), end(buf));`
	template<typename T>
	inline static const T*
	begin(const Buf<T>& self)
	{
		return buf_begin(self);
	}

	// begin function overload which is useful when using range for loop ex. `for (auto x: buf)` and various standard
	// algorithms ex. `std::sort(begin(buf), end(buf));`
	template<typename T>
	inline static T*
	begin(Buf<T>& self)
	{
		return buf_begin(self);
	}

	// end function overload which is useful when using range for loop ex. `for (auto x: buf)` and various standard
	// algorithms ex. `std::sort(begin(buf), end(buf));`
	template<typename T>
	inline static const T*
	end(const Buf<T>& self)
	{
		return buf_end(self);
	}

	// end function overload which is useful when using range for loop ex. `for (auto x: buf)` and various standard
	// algorithms ex. `std::sort(begin(buf), end(buf));`
	template<typename T>
	inline static T*
	end(Buf<T>& self)
	{
		return buf_end(self);
	}

	// general clone function which returns a new equal and disjoint instance of the given value
	template<typename T>
	inline static T
	clone(const T& value)
	{
		if constexpr (std::is_integral_v<T>)
		{
			return value;
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			return value;
		}
		else if constexpr (std::is_enum_v<T>)
		{
			return value;
		}
		else if constexpr (std::is_pointer_v<T>)
		{
			return value;
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			return value;
		}
		else if constexpr (std::is_same_v<T, char>)
		{
			return value;
		}
		else
		{
			static_assert(sizeof(T) == 0, "no clone function found for this type");
			return value;
		}
	}

	// a custom clone function for the buf, which iterators over the elements and calls clone on each one of them
	// thus making a deep copy of the buf
	template<typename T>
	inline static Buf<T>
	buf_clone(const Buf<T>& other, Allocator allocator = allocator_top())
	{
		Buf<T> buf = buf_with_allocator<T>(allocator);
		buf_resize(buf, other.count);
		for(size_t i = 0; i < other.count; ++i)
			buf[i] = clone(other[i]);
		return buf;
	}

	// an overload of the general clone function which uses the custom clone function of the buf which iterators over
	// the elements and calls clone on each one of them thus making a deep copy of the buf
	template<typename T>
	inline static Buf<T>
	clone(const Buf<T>& other)
	{
		return buf_clone(other);
	}

	// memcpys the underlying memory of the given buf, this is useful as a fast copy for tivially copyable types
	template<typename T>
	inline static Buf<T>
	buf_memcpy_clone(const Buf<T>& other, Allocator allocator = allocator_top())
	{
		Buf<T> buf = buf_with_allocator<T>(allocator);
		buf_resize(buf, other.count);
		::memcpy(buf.ptr, other.ptr, buf.count * sizeof(T));
		return buf;
	}

	// general clone functions for builtin types
	#define TRIVIAL_MEMCPY_CLONE(TYPE) \
	inline static Buf<TYPE> \
	clone(const Buf<TYPE>& other) \
	{ \
		return buf_memcpy_clone(other); \
	}

	TRIVIAL_MEMCPY_CLONE(bool)
	TRIVIAL_MEMCPY_CLONE(int8_t)
	TRIVIAL_MEMCPY_CLONE(int16_t)
	TRIVIAL_MEMCPY_CLONE(int32_t)
	TRIVIAL_MEMCPY_CLONE(int64_t)
	TRIVIAL_MEMCPY_CLONE(uint8_t)
	TRIVIAL_MEMCPY_CLONE(uint16_t)
	TRIVIAL_MEMCPY_CLONE(uint32_t)
	TRIVIAL_MEMCPY_CLONE(uint64_t)
	TRIVIAL_MEMCPY_CLONE(float)
	TRIVIAL_MEMCPY_CLONE(double)

	#undef TRIVIAL_MEMCPY_CLONE

	// returns a block of memory which encloses the given buf elements
	template<typename T>
	inline static Block
	block_from(const Buf<T>& b)
	{
		return Block{ b.ptr, b.count * sizeof(T) };
	}
}
