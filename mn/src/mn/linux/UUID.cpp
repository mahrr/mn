#include "mn/UUID.h"

#include <sys/random.h>

namespace mn
{
	inline static void
	_crypto_rand(mn::Block buffer)
	{
		size_t s = 0;
		while (s < buffer.size)
		{
			s += getrandom(buffer.ptr, buffer.size - s, 0);
		}
	}

	inline static UUID
	_rand_uuid()
	{
		UUID self{};
		_crypto_rand({&self, sizeof(self)});
		// version 4
		self.bytes[6] = (self.bytes[6] & 0x0f) | 0x40;
		// variant is 10
		self.bytes[8] = (self.bytes[8] & 0x3f) | 0x80;
		return self;
	}

	UUID
	uuid_generate()
	{
		return _rand_uuid();
	}
} // namespace mn
