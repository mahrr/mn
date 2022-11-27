#include "mn/Stream.h"

#include <mn/Memory.h>

namespace mn
{
	//API
	Result<size_t, IO_ERROR>
	stream_read(Stream self, Block data)
	{
		return self->read(data);
	}

	Result<size_t, IO_ERROR>
	stream_write(Stream self, Block data)
	{
		return self->write(data);
	}

	Result<size_t, IO_ERROR>
	stream_size(Stream self)
	{
		return self->size();
	}

	void
	stream_free(Stream self)
	{
		self->dispose();
	}
}