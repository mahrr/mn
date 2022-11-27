#pragma once

#include "mn/Exports.h"
#include "mn/Stream.h"
#include "mn/Str.h"

namespace mn
{
	// an in memory byte stream
	typedef struct IMemory_Stream* Memory_Stream;

	// some forward declaration because this language require this kind of thing

	MN_EXPORT void
	memory_stream_cursor_move(Memory_Stream self, int64_t offset);

	MN_EXPORT void
	memory_stream_cursor_set(Memory_Stream self, int64_t abs);

	struct IMemory_Stream final: IStream
	{
		Str str;
		int64_t cursor;

		MN_EXPORT virtual void
		dispose() override;

		MN_EXPORT virtual Result<size_t, IO_ERROR>
		read(Block data) override;

		MN_EXPORT virtual Result<size_t, IO_ERROR>
		write(Block data) override;

		MN_EXPORT virtual Result<size_t, IO_ERROR>
		size() override;

		virtual Result<size_t, IO_ERROR>
		cursor_operation(STREAM_CURSOR_OP op, int64_t arg) override
		{
			switch (op)
			{
			case STREAM_CURSOR_GET:
				return this->cursor;
			case STREAM_CURSOR_MOVE:
				memory_stream_cursor_move(this, arg);
				return this->cursor;
			case STREAM_CURSOR_SET:
				memory_stream_cursor_set(this, arg);
				return this->cursor;
			case STREAM_CURSOR_START:
				this->cursor = 0;
				return 0;
			case STREAM_CURSOR_END:
				this->cursor = this->str.count;
				return this->cursor;
			default:
				mn_unreachable();
				return IO_ERROR_UNKNOWN;
			}
		}
	};

	// creates a new memory stream with the given allocator
	MN_EXPORT Memory_Stream
	memory_stream_new(Allocator allocator = allocator_top());

	// frees the given memory stream
	MN_EXPORT void
	memory_stream_free(Memory_Stream self);

	// destruct overload for memory stream free
	inline static void
	destruct(Memory_Stream self)
	{
		memory_stream_free(self);
	}

	// writes the given block of bytes to a memory stream, returns the amount of written bytes
	MN_EXPORT Result<size_t, IO_ERROR>
	memory_stream_write(Memory_Stream self, Block data);

	// reads into the given memory block from a memory stream, returns the amount of read bytes
	MN_EXPORT Result<size_t, IO_ERROR>
	memory_stream_read(Memory_Stream self, Block data);

	// returns the size of the given memory stream
	MN_EXPORT Result<size_t, IO_ERROR>
	memory_stream_size(Memory_Stream self);

	// returns whether stream's cursor has reached end of file
	MN_EXPORT bool
	memory_stream_eof(Memory_Stream self);

	// returns the position of stream's cursor
	MN_EXPORT Result<size_t, IO_ERROR>
	memory_stream_cursor_pos(Memory_Stream self);

	// moves the cursor by the given offset
	MN_EXPORT void
	memory_stream_cursor_move(Memory_Stream self, int64_t offset);

	// sets the cursor to the given absolute position
	MN_EXPORT void
	memory_stream_cursor_set(Memory_Stream self, int64_t abs);

	// sets stream's cursor to the start
	MN_EXPORT void
	memory_stream_cursor_to_start(Memory_Stream self);

	// sets stream's cursor to the end
	MN_EXPORT void
	memory_stream_cursor_to_end(Memory_Stream self);

	// ensures that stream's capacity can accomodate the given size of bytes
	MN_EXPORT void
	memory_stream_reserve(Memory_Stream self, size_t size);

	// returns the capacity of the given stream
	MN_EXPORT size_t
	memory_stream_capacity(Memory_Stream self);

	// clears the given memory stream
	MN_EXPORT void
	memory_stream_clear(Memory_Stream self);

	// retursn the memory block starting from the cursor with the given size(in bytes) advancing
	// towards the end of the stream
	// memory_stream_block_ahead([abcd|efghe], 2) -> [ef]
	MN_EXPORT Block
	memory_stream_block_ahead(Memory_Stream self, size_t size);

	// returns the memory block starting from the cursor with the given
	// size(in bytes) advancing towards the start of the stream
	// memory_stream_block_behind([abcd|efghe], 2) -> [cd]
	MN_EXPORT Block
	memory_stream_block_behind(Memory_Stream self, size_t size);

	// pipes data into the memory stream with the given size, returns the amount of written bytes
	MN_EXPORT size_t
	memory_stream_pipe(Memory_Stream self, Stream stream, size_t size);

	// returns a pointer to the content of the memory stream
	inline static const char*
	memory_stream_ptr(Memory_Stream self)
	{
		return self->str.ptr;
	}

	// returns the content of the memory stream as a string, and clears the given memory stream
	inline static Str
	memory_stream_str(Memory_Stream self)
	{
		Str res = self->str;
		self->str = str_with_allocator(self->str.allocator);
		self->cursor = 0;
		return res;
	}
}