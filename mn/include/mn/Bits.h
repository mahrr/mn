#pragma once

#include "mn/Exports.h"

#include <stdint.h>

namespace mn
{
	enum ENDIAN
	{
		ENDIAN_LITTLE,
		ENDIAN_BIG,
	};

	MN_EXPORT ENDIAN
	system_endianness();

	MN_EXPORT uint16_t
	byteswap_uint16(uint16_t v);

	MN_EXPORT uint32_t
	byteswap_uint32(uint32_t v);

	MN_EXPORT uint64_t
	byteswap_uint64(uint64_t v);

	MN_EXPORT int
	leading_zeros(uint64_t v);
}