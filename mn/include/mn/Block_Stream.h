#pragma once

#include "mn/Exports.h"
#include "mn/Stream.h"

#include <string.h>

namespace mn
{
	struct Block_Stream;

	// just forward declaration because this language require this kind of thing
	inline static Result<size_t, IO_ERROR>
	block_stream_read(Block_Stream& self, Block data);

	// wraps a stream interface around a simple memory block, which allows you to use standard read, write, and cursor
	// operation over a block of memory
	struct Block_Stream final : IStream
	{
		Block data;
		int64_t cursor;

		// frees the given block stream
		void
		dispose() override
		{}

		// reads into the given memory block and returns the amount of read memory in bytes
		Result<size_t, IO_ERROR>
		read(Block out_data) override
		{
			return block_stream_read(*this, out_data);
		}

		// block stream is a readonly one so it fails when you write into it by returning 0 bytes written
		Result<size_t, IO_ERROR>
		write(Block) override
		{
			return IO_ERROR_NOT_SUPPORTED;
		}

		// returns the size of the stream
		Result<size_t, IO_ERROR>
		size() override
		{
			return data.size;
		}

		// performs the given operation on the cursor and returns the new value of the cursor
		Result<size_t, IO_ERROR>
		cursor_operation(STREAM_CURSOR_OP op, int64_t arg) override
		{
			switch (op)
			{
			case STREAM_CURSOR_GET:
				return this->cursor;
			case STREAM_CURSOR_MOVE:
				this->cursor += arg;
				return this->cursor;
			case STREAM_CURSOR_SET:
				this->cursor = arg;
				return this->cursor;
			case STREAM_CURSOR_START:
				this->cursor = 0;
				return 0;
			case STREAM_CURSOR_END:
				this->cursor = this->data.size;
				return this->cursor;
			default:
				mn_unreachable();
				return IO_ERROR_UNKNOWN;
			}
		}
	};

	// wraps the given memory block into a block stream
	inline static Block_Stream
	block_stream_wrap(Block data)
	{
		Block_Stream self{};
		self.data = data;
		return self;
	}

	// reads from the block stream into the given memory block and returns the amount of read data in bytes
	inline static Result<size_t, IO_ERROR>
	block_stream_read(Block_Stream& self, Block data)
	{
		if (size_t(self.cursor) >= self.data.size)
			return IO_ERROR_END_OF_FILE;

		size_t available_size = self.data.size - self.cursor;
		size_t read_size = available_size > data.size ? data.size : available_size;
		::memcpy(data.ptr, (char*)self.data.ptr + self.cursor, read_size);
		self.cursor += read_size;
		return read_size;
	}

	// returns the size of the block stream in bytes
	inline static size_t
	block_stream_size(const Block_Stream& self)
	{
		return self.data.size;
	}

	// returns the current position of stream's cursor
	inline static size_t
	block_stream_cursor_pos(const Block_Stream& self)
	{
		return self.cursor;
	}

	// moves the cursor position with the given offset, which is signed to allow you to move forwards and backwards
	inline static void
	block_stream_cursor_move(Block_Stream& self, int64_t offset)
	{
		self.cursor += offset;
	}

	// sets the cursor position to an absolute position
	inline static void
	block_stream_cursor_set(Block_Stream& self, int64_t abs)
	{
		self.cursor = abs;
	}

	// resets the cursor position back to the start of the stream
	inline static void
	block_stream_cursor_to_start(Block_Stream& self)
	{
		self.cursor = 0;
	}

	// sets the cursor position to the end of the stream
	inline static void
	block_stream_cursor_to_end(Block_Stream& self)
	{
		self.cursor = self.data.size;
	}
}
