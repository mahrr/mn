#include "mn/UUID.h"

#include <sys/random.h>

namespace mn
{
	inline static bool
	_crypto_rand(Block buffer)
	{
		return getentropy(buffer.ptr, buffer.size) == 0;
	}

	inline static UUID
	_rand_uuid()
	{
		UUID self{};
		[[maybe_unused]] auto res = _crypto_rand({&self, sizeof(self)});
		mn_assert(res);
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