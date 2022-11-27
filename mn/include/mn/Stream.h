#pragma once

#include "mn/Exports.h"
#include "mn/Base.h"
#include "mn/Str.h"
#include "mn/Result.h"

#include <stdint.h>

namespace mn
{
	enum IO_ERROR
	{
		IO_ERROR_NONE,
		IO_ERROR_NOT_SUPPORTED,
		IO_ERROR_END_OF_FILE,
		IO_ERROR_PERMISSION_DENIED,
		IO_ERROR_CLOSED,
		IO_ERROR_TIMEOUT,
		IO_ERROR_OUT_OF_MEMORY,
		IO_ERROR_INTERNAL_ERROR,
		IO_ERROR_UNKNOWN,
	};

	inline static const char*
	io_error_message(IO_ERROR e)
	{
		switch (e)
		{
		case IO_ERROR_NONE: return "no error";
		case IO_ERROR_NOT_SUPPORTED: return "operation is not supported";
		case IO_ERROR_END_OF_FILE: return "end of file";
		case IO_ERROR_PERMISSION_DENIED: return "permission denied";
		case IO_ERROR_CLOSED: return "connection closed";
		case IO_ERROR_TIMEOUT: return "timeout";
		case IO_ERROR_OUT_OF_MEMORY: return "out of memory";
		case IO_ERROR_INTERNAL_ERROR: return "internal error";
		case IO_ERROR_UNKNOWN: return "generic error";
		default: mn_unreachable(); return "<UNKNOWN ERROR>";
		}
	}

	enum STREAM_CURSOR_OP
	{
		STREAM_CURSOR_GET,
		STREAM_CURSOR_MOVE,
		STREAM_CURSOR_SET,
		STREAM_CURSOR_START,
		STREAM_CURSOR_END,
	};

	// a generic stream handle
	typedef struct IStream* Stream;

	struct IStream
	{
		virtual void dispose() = 0;
		virtual mn::Result<size_t, IO_ERROR> read(Block data) = 0;
		virtual mn::Result<size_t, IO_ERROR> write(Block data) = 0;
		virtual mn::Result<size_t, IO_ERROR> size() = 0;
		virtual mn::Result<size_t, IO_ERROR> cursor_operation(STREAM_CURSOR_OP op, int64_t offset) = 0;
	};

	// reads from stream into the given bytes block and returns the number of read bytes
	MN_EXPORT mn::Result<size_t, IO_ERROR>
	stream_read(Stream self, Block data);

	// reads to stream from the given bytes block and returns the number of written bytes
	MN_EXPORT mn::Result<size_t, IO_ERROR>
	stream_write(Stream self, Block data);

	// returns size of the stream
	MN_EXPORT mn::Result<size_t, IO_ERROR>
	stream_size(Stream self);

	// frees the given stream
	MN_EXPORT void
	stream_free(Stream self);

	// destruct overload for stream free
	inline static void
	destruct(Stream self)
	{
		stream_free(self);
	}

	// returns stream's cursor position
	inline static mn::Result<size_t, IO_ERROR>
	stream_cursor_pos(IStream* self)
	{
		return self->cursor_operation(STREAM_CURSOR_GET, 0);
	}

	// moves the stream by the given offset and returns the new position
	inline static mn::Result<size_t, IO_ERROR>
	stream_cursor_move(IStream* self, int64_t offset)
	{
		return self->cursor_operation(STREAM_CURSOR_MOVE, offset);
	}

	// sets stream cursor to the given absolute position and returns the new position
	inline static mn::Result<size_t, IO_ERROR>
	stream_cursor_set(IStream* self, int64_t abs)
	{
		return self->cursor_operation(STREAM_CURSOR_SET, abs);
	}

	// sets stream cursor to the start and returns the new position
	inline static mn::Result<size_t, IO_ERROR>
	stream_cursor_to_start(IStream* self)
	{
		return self->cursor_operation(STREAM_CURSOR_START, 0);
	}

	// sets stream cursor to the end and returns the new position
	inline static mn::Result<size_t, IO_ERROR>
	stream_cursor_to_end(IStream* self)
	{
		return self->cursor_operation(STREAM_CURSOR_END, 0);
	}

	// copies bytes from src to dst stream, returns the number of copied bytes
	inline static Result<size_t, IO_ERROR>
	stream_copy(IStream* dst, IStream* src)
	{
		size_t res = 0;
		char _buf[1024];
		auto buf = block_from(_buf);
		while(true)
		{
			auto [read_size, read_err] = src->read(buf);
			if (read_err != IO_ERROR_NONE)
				return read_err;

			auto ptr = (char*)buf.ptr;
			auto size = read_size;
			while (size > 0)
			{
				auto [write_size, write_err] = dst->write(Block{ptr, size});
				if (write_err != IO_ERROR_NONE)
					return write_err;
				size -= write_size;
				ptr += write_size;
				res += write_size;
			}
		}
		return res;
	}

	// copies bytes from the src stream into the dst block, returns the number of copied bytes
	inline static Result<size_t, IO_ERROR>
	stream_copy(Block dst, IStream* src)
	{
		size_t res = 0;
		auto ptr = (char*)dst.ptr;
		auto size = dst.size;
		while(size > 0)
		{
			auto [read_size, read_err] = src->read(Block{ptr, size});
			if (read_err != IO_ERROR_NONE)
				return read_err;

			ptr += read_size;
			size -= read_size;
			res += read_size;
		}
		return res;
	}

	// copies bytes from the src block into the dst stream, returns the number of copied bytes
	inline static Result<size_t, IO_ERROR>
	stream_copy(IStream* dst, Block src)
	{
		size_t res = 0;
		auto ptr = (char*)src.ptr;
		auto size = src.size;
		while(size > 0)
		{
			auto [write_size, write_err] = dst->write(Block{ptr, size});
			if (write_err != IO_ERROR_NONE)
				return write_err;

			ptr += write_size;
			size -= write_size;
			res += write_size;
		}
		return res;
	}

	// reads as much as possible (until the stream reads 0 bytes) from the given stream into a string
	inline static Result<Str, IO_ERROR>
	stream_sink(IStream* src, Allocator allocator = allocator_top())
	{
		auto res = str_with_allocator(allocator);
		char _buf[1024];
		auto buf = block_from(_buf);
		while(true)
		{
			auto [read_size, read_err] = src->read(buf);
			if (read_err != IO_ERROR_NONE)
				return read_err;

			str_block_push(res, Block{buf.ptr, read_size});
		}
		return res;
	}
}