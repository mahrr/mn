#include "mn/Memory_Stream.h"
#include "mn/Assert.h"

namespace mn
{
	//API
	void
	IMemory_Stream::dispose()
	{
		str_free(str);
		free_from(str.allocator, this);
	}

	Result<size_t, IO_ERROR>
	IMemory_Stream::read(Block data)
	{
		mn_assert_msg(cursor >= 0, "Memory_Stream cursor is not valid");
		mn_assert_msg(cursor <= int64_t(str.count), "Memory_Stream cursor is not valid");
		size_t available_size = str.count - size_t(cursor);
		if(available_size)
		{
			available_size = available_size > data.size ? data.size : available_size;
			::memcpy(data.ptr, str.ptr + cursor, available_size);
			cursor += available_size;
		}
		return available_size;
	}

	Result<size_t, IO_ERROR>
	IMemory_Stream::write(Block data)
	{
		mn_assert_msg(cursor >= 0, "Memory_Stream cursor is not valid");
		buf_resize(str, cursor + data.size);
		::memcpy(str.ptr + cursor, data.ptr, data.size);
		str_null_terminate(str);
		cursor += data.size;
		return data.size;
	}

	Result<size_t, IO_ERROR>
	IMemory_Stream::size()
	{
		return str.count;
	}


	Memory_Stream
	memory_stream_new(Allocator allocator)
	{
		Memory_Stream self = alloc_construct_from<IMemory_Stream>(allocator);
		self->str = str_with_allocator(allocator);
		self->cursor = 0;
		return self;
	}

	void
	memory_stream_free(Memory_Stream self)
	{
		self->dispose();
	}

	Result<size_t, IO_ERROR>
	memory_stream_write(Memory_Stream self, Block data)
	{
		return self->write(data);
	}

	Result<size_t, IO_ERROR>
	memory_stream_read(Memory_Stream self, Block data)
	{
		return self->read(data);
	}

	Result<size_t, IO_ERROR>
	memory_stream_size(Memory_Stream self)
	{
		return self->str.count;
	}

	bool
	memory_stream_eof(Memory_Stream self)
	{
		return self->cursor >= int64_t(self->str.count);
	}

	Result<size_t, IO_ERROR>
	memory_stream_cursor_pos(Memory_Stream self)
	{
		return self->cursor;
	}

	void
	memory_stream_cursor_move(Memory_Stream self, int64_t offset)
	{
		mn_assert_msg(self->cursor + offset >= 0, "Memory_Stream cursor is not valid");
		mn_assert_msg(size_t(self->cursor + offset) <= self->str.count, "Memory_Stream cursor is not valid");
		self->cursor += offset;
	}

	void
	memory_stream_cursor_set(Memory_Stream self, int64_t abs)
	{
		mn_assert(abs >= 0);
		mn_assert(size_t(abs) <= self->str.count);
		self->cursor = abs;
	}

	void
	memory_stream_cursor_to_start(Memory_Stream self)
	{
		self->cursor = 0;
	}

	void
	memory_stream_cursor_to_end(Memory_Stream self)
	{
		self->cursor = self->str.count;
	}

	void
	memory_stream_reserve(Memory_Stream self, size_t size)
	{
		buf_reserve(self->str, size);
	}

	size_t
	memory_stream_capacity(Memory_Stream self)
	{
		return self->str.cap;
	}

	void
	memory_stream_clear(Memory_Stream self)
	{
		str_clear(self->str);
		self->cursor = 0;
	}

	Block
	memory_stream_block_ahead(Memory_Stream self, size_t size)
	{
		if(size == 0)
			size = self->str.count - self->cursor;
		mn_assert(size <= self->str.count - self->cursor);
		return Block { self->str.ptr + self->cursor, size };
	}

	Block
	memory_stream_block_behind(Memory_Stream self, size_t size)
	{
		if(size == 0)
			size = self->cursor;
		mn_assert(size <= size_t(self->cursor));
		return Block { self->str.ptr, size };
	}

	size_t
	memory_stream_pipe(Memory_Stream self, Stream stream, size_t size)
	{
		if(self->str.count - self->cursor < size)
			memory_stream_reserve(self, size);

		auto [read_size, _] = stream_read(stream, Block { self->str.ptr + self->cursor, size });
		self->str.count += read_size;
		self->cursor += read_size;
		return read_size;
	}
}