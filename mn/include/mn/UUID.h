#pragma once

#include "mn/Exports.h"
#include "mn/IO.h"
#include "mn/Map.h"
#include "mn/Result.h"

#include <stdint.h>

namespace mn
{
	// a universally unique identifier
	union UUID
	{
		struct
		{
			uint32_t time_low;
			uint16_t time_mid;
			uint16_t time_hi_and_version;
			uint8_t clk_seq_hi_res;
			uint8_t clk_seq_low;
			uint8_t node[6];
		} parts;
		uint64_t rnd[2];
		uint8_t bytes[16];
	};

	// empty uuid
	inline UUID null_uuid{};

	inline static bool
	operator==(const UUID &a, const UUID &b)
	{
		return (a.rnd[0] == b.rnd[0]) && (a.rnd[1] == b.rnd[1]);
	}

	inline static bool
	operator!=(const UUID &a, const UUID &b)
	{
		return !operator==(a, b);
	}

	// generates a new uuid using OS provided interface
	MN_EXPORT UUID
	uuid_generate();

	inline static uint8_t
	_uuid_hex_to_uint8(char c)
	{
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'a' && c <= 'f')
			return 10 + c - 'a';
		if (c >= 'A' && c <= 'F')
			return 10 + c - 'A';
		return 0;
	}

	inline static bool
	_uuid_char_is_hex(char c)
	{
		return (c >= '0' && c <= '9') ||
			   (c >= 'a' && c <= 'f') ||
			   (c >= 'A' && c <= 'F');
	}

	inline static uint8_t
	_uuid_hex_to_uint8(char a, char b)
	{
		return (_uuid_hex_to_uint8(a) << 4) | _uuid_hex_to_uint8(b);
	}

	// parses a uuid from a string, returns either a valid uuid or an error
	inline static Result<UUID>
	uuid_parse(const char* str)
	{
		UUID self{};
		auto size = ::strlen(str);

		if (str == nullptr || size == 0)
			return mn::errf("empty string");

		size_t has_braces = 0;
		if (str[0] == '{')
			has_braces = 1;

		if (has_braces > 0 && str[size - 1] != '}')
			return mn::errf("mismatched opening curly brace");

		size_t index = 0;
		bool first_digit = true;
		char digit = 0;
		for (size_t i = has_braces; i < size - has_braces; ++i)
		{
			if (str[i] == '-')
				continue;

			if (index >= 16 || _uuid_char_is_hex(str[i]) == false)
				return mn::errf("invalid uuid");

			if (first_digit)
			{
				digit = str[i];
				first_digit = false;
			}
			else
			{
				self.bytes[index++] = _uuid_hex_to_uint8(digit, str[i]);
				first_digit = true;
			}
		}

		if (index < 16)
			return mn::errf("invalid uuid");

		return self;
	}

	// parses a uuid from a string, returns either a valid uuid or an error
	inline static Result<UUID>
	uuid_parse(const Str& s)
	{
		return uuid_parse(s.ptr);
	}

	// indicated by a bit pattern in octet 8, marked with N in xxxxxxxx-xxxx-xxxx-Nxxx-xxxxxxxxxxxx
	enum UUID_VARIANT
	{
		// NCS backward compatibility (with the obsolete Apollo Network Computing System 1.5 UUID format)
		// N bit pattern: 0xxx
		// > the first 6 octets of the UUID are a 48-bit timestamp (the number of 4 microsecond units of time since 1 Jan 1980 UTC);
		// > the next 2 octets are reserved;
		// > the next octet is the "address family";
		// > the final 7 octets are a 56-bit host ID in the form specified by the address family
		UUID_VARIANT_NCS,

		// RFC 4122/DCE 1.1
		// N bit pattern: 10xx
		// > big-endian byte order
		UUID_VARIANT_RFC,

		// Microsoft Corporation backward compatibility
		// N bit pattern: 110x
		// > little endian byte order
		// > formely used in the Component Object Model (COM) library
		UUID_VARIANT_MICROSOFT,

		// reserved for possible future definition
		// N bit pattern: 111x
		UUID_VARIANT_RESERVED,
	};

	// returns which variant this uuid uses
	inline static UUID_VARIANT
	uuid_variant(const UUID& self)
	{
		if ((self.bytes[8] & 0x80) == 0x00)
			return UUID_VARIANT_NCS;
		 else if ((self.bytes[8] & 0xC0) == 0x80)
			return UUID_VARIANT_RFC;
		 else if ((self.bytes[8] & 0xE0) == 0xC0)
			return UUID_VARIANT_MICROSOFT;
		 else
			return UUID_VARIANT_RESERVED;
	}

	enum UUID_VERSION
	{
		// only possible for nil or invalid uuids
		UUID_VERSION_NONE,
		// The time-based version specified in RFC 4122
		UUID_VERSION_TIME_BASED,
		// DCE Security version, with embedded POSIX UIDs.
		UUID_VERSION_DCE_SECURITY,
		// The name-based version specified in RFS 4122 with MD5 hashing
		UUID_VERSION_NAME_BASED_MD5,
		// The randomly or pseudo-randomly generated version specified in RFS 4122
		UUID_VERSION_RANDOM_NUMBER_BASED,
		// The name-based version specified in RFS 4122 with SHA1 hashing
		UUID_VERSION_NAME_BASED_SHA1,
	};

	// returns the version of the given uuid
	inline static UUID_VERSION
	uuid_version(const UUID& self)
	{
		if ((self.bytes[6] & 0xF0) == 0x10)
			return UUID_VERSION_TIME_BASED;
		else if ((self.bytes[6] & 0xF0) == 0x20)
			return UUID_VERSION_DCE_SECURITY;
		else if ((self.bytes[6] & 0xF0) == 0x30)
			return UUID_VERSION_NAME_BASED_MD5;
		else if ((self.bytes[6] & 0xF0) == 0x40)
			return UUID_VERSION_RANDOM_NUMBER_BASED;
		else if ((self.bytes[6] & 0xF0) == 0x50)
			return UUID_VERSION_NAME_BASED_SHA1;
		else
			return UUID_VERSION_NONE;
	}

	template<>
	struct Hash<UUID>
	{
		size_t
		operator()(const UUID &v) const
		{
			return murmur_hash(Block{(void *)v.bytes, size_t(sizeof(v.bytes))});
		}
	};
} // namespace mn

namespace fmt
{
	template<>
	struct formatter<mn::UUID>
	{
		template<typename ParseContext>
		constexpr auto
		parse(ParseContext &ctx)
		{
			return ctx.begin();
		}

		template<typename FormatContext>
		auto
		format(const mn::UUID &self, FormatContext &ctx)
		{
			return format_to(
				ctx.out(),
				"{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
				self.bytes[0],
				self.bytes[1],
				self.bytes[2],
				self.bytes[3],
				self.bytes[4],
				self.bytes[5],
				self.bytes[6],
				self.bytes[7],
				self.bytes[8],
				self.bytes[9],
				self.bytes[10],
				self.bytes[11],
				self.bytes[12],
				self.bytes[13],
				self.bytes[14],
				self.bytes[15]
			);
		}
	};
} // namespace fmt