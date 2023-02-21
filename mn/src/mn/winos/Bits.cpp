#include "mn/Bits.h"

#include <intrin.h>

namespace mn
{
	static union
	{
		char c[4];
		uint32_t dword;
	} ENDIAN_TEST = {{'l', '?', '?', 'b'}};

	// API
	ENDIAN
	system_endianness()
	{
		static ENDIAN _endian = char(ENDIAN_TEST.dword) == 'l' ? ENDIAN_LITTLE : ENDIAN_BIG;
		return _endian;
	}

	uint16_t
	byteswap_uint16(uint16_t v)
	{
		return _byteswap_ushort(v);
	}

	uint32_t
	byteswap_uint32(uint32_t v)
	{
		return _byteswap_ulong(v);
	}

	uint64_t
	byteswap_uint64(uint64_t v)
	{
		return _byteswap_uint64(v);
	}

	int
	leading_zeros(uint64_t v)
	{
		unsigned long result = 0;
		if (_BitScanReverse64(&result, v))
		{
			return 63 - result;
		}
		else
		{
			return 64;
		}
	}
}