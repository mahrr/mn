#pragma once

#include "mn/Memory_Stream.h"
#include "mn/Buf.h"
#include "mn/Str.h"
#include "mn/Map.h"
#include "mn/Defer.h"
#include "mn/Block_Stream.h"

namespace mn
{
	struct Msgpack_Writer
	{
		Memory_Stream stream;
	};

	inline static Msgpack_Writer
	msgpack_writer_new()
	{
		Msgpack_Writer self{};
		self.stream = memory_stream_new();
		return self;
	}

	inline static void
	msgpack_writer_free(Msgpack_Writer& self)
	{
		memory_stream_free(self.stream);
	}

	inline static Err
	_msgpack_push(Msgpack_Writer& self, Block v)
	{
		auto [size, err] = stream_copy(self.stream, v);
		if (err)
			return errf("failed to write into memory stream, {}", io_error_message(err));
		if (size != v.size)
			return errf("failed to write {} bytes, only {} was written", v.size, size);
		return {};
	}

	template<typename T>
	inline static Err
	_msgpack_push(Msgpack_Writer& self, T v)
	{
		return _msgpack_push(self, Block{&v, sizeof(v)});
	}

	inline static Err
	msgpack(Msgpack_Writer& self, nullptr_t)
	{
		return _msgpack_push(self, 0xc0);
	}

	inline static Err
	msgpack(Msgpack_Writer& self, bool value)
	{
		uint8_t rep{};
		if (value)
			rep = 0xc3;
		else
			rep = 0xc2;
		return _msgpack_push(self, rep);
	}

	inline static Err
	msgpack(Msgpack_Writer& self, uint64_t v)
	{
		if (v <= 0x7f)
		{
			uint8_t rep = (uint8_t)v;
			return _msgpack_push(self, rep);
		}
		else if (v <= UINT8_MAX)
		{
			uint8_t prefix = 0xcc;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t num = (uint8_t)v;
			return _msgpack_push(self, num);
		}
		else if (v <= UINT16_MAX)
		{
			uint8_t prefix = 0xcd;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint16_t num = (uint16_t)v;
			return _msgpack_push(self, num);
		}
		else if (v <= UINT32_MAX)
		{
			uint8_t prefix = 0xce;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint32_t num = (uint32_t)v;
			return _msgpack_push(self, num);
		}
		else if (v <= UINT64_MAX)
		{
			uint8_t prefix = 0xcf;
			if (auto err = _msgpack_push(self, prefix)) return err;
			return _msgpack_push(self, v);
		}
		else
		{
			mn_unreachable();
			return errf("integers larger than 64 bit is not supported");
		}
	}

	inline static Err
	msgpack(Msgpack_Writer& self, uint8_t v)
	{
		return msgpack(self, (uint64_t)v);
	}

	inline static Err
	msgpack(Msgpack_Writer& self, uint16_t v)
	{
		return msgpack(self, (uint64_t)v);
	}

	inline static Err
	msgpack(Msgpack_Writer& self, uint32_t v)
	{
		return msgpack(self, (uint64_t)v);
	}

	inline static Err
	msgpack(Msgpack_Writer& self, int64_t v)
	{
		if (v >= 0)
		{
			if (v <= 0x7f)
			{
				int8_t rep = (int8_t)v;
				return _msgpack_push(self, rep);
			}
			else if (v <= INT8_MAX)
			{
				uint8_t prefix = 0xd0;
				if (auto err = _msgpack_push(self, prefix)) return err;
				int8_t num = (int8_t)v;
				return _msgpack_push(self, num);
			}
			else if (v <= INT16_MAX)
			{
				uint8_t prefix = 0xd1;
				if (auto err = _msgpack_push(self, prefix)) return err;
				int16_t num = (int16_t)v;
				return _msgpack_push(self, num);
			}
			else if (v <= INT32_MAX)
			{
				uint8_t prefix = 0xd2;
				if (auto err = _msgpack_push(self, prefix)) return err;
				int32_t num = (int32_t)v;
				return _msgpack_push(self, num);
			}
			else if (v <= INT64_MAX)
			{
				uint8_t prefix = 0xd3;
				if (auto err = _msgpack_push(self, prefix)) return err;
				return _msgpack_push(self, v);
			}
			else
			{
				mn_unreachable();
				return errf("integers larger than 64 bit is not supported");
			}
		}
		else
		{
			if (v >= -32)
			{
				int8_t rep = (int8_t)v;
				return _msgpack_push(self, rep);
			}
			else if (v >= INT8_MIN)
			{
				uint8_t prefix = 0xd0;
				if (auto err = _msgpack_push(self, prefix)) return err;
				int8_t num = (int8_t)v;
				return _msgpack_push(self, num);
			}
			else if (v >= INT16_MIN)
			{
				uint8_t prefix = 0xd1;
				if (auto err = _msgpack_push(self, prefix)) return err;
				int16_t num = (int16_t)v;
				return _msgpack_push(self, num);
			}
			else if (v >= INT32_MIN)
			{
				uint8_t prefix = 0xd2;
				if (auto err = _msgpack_push(self, prefix)) return err;
				int32_t num = (int32_t)v;
				return _msgpack_push(self, num);
			}
			else if (v >= INT64_MIN)
			{
				uint8_t prefix = 0xd3;
				if (auto err = _msgpack_push(self, prefix)) return err;
				return _msgpack_push(self, v);
			}
			else
			{
				mn_unreachable();
				return errf("integers larger than 64 bit is not supported");
			}
		}
	}

	inline static Err
	msgpack(Msgpack_Writer& self, int8_t v)
	{
		return msgpack(self, (int64_t)v);
	}

	inline static Err
	msgpack(Msgpack_Writer& self, int16_t v)
	{
		return msgpack(self, (int64_t)v);
	}

	inline static Err
	msgpack(Msgpack_Writer& self, int32_t v)
	{
		return msgpack(self, (int64_t)v);
	}

	inline static Err
	msgpack(Msgpack_Writer& self, float v)
	{
		uint8_t prefix = 0xca;
		if (auto err = _msgpack_push(self, prefix)) return err;
		return _msgpack_push(self, v);
	}

	inline static Err
	msgpack(Msgpack_Writer& self, double v)
	{
		uint8_t prefix = 0xcb;
		if (auto err = _msgpack_push(self, prefix)) return err;
		return _msgpack_push(self, v);
	}

	inline static Err
	msgpack(Msgpack_Writer& self, const Str& v)
	{
		if (v.count <= 31)
		{
			uint8_t prefix = (uint8_t)v.count;
			prefix |= 0xa0;
			if (auto err = _msgpack_push(self, prefix)) return err;
			return _msgpack_push(self, block_from(v));
		}
		else if (v.count <= UINT8_MAX)
		{
			uint8_t prefix = 0xd9;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)v.count;
			if (auto err = _msgpack_push(self, count)) return err;
			return _msgpack_push(self, block_from(v));
		}
		else if (v.count <= UINT16_MAX)
		{
			uint8_t prefix = 0xda;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)v.count;
			if (auto err = _msgpack_push(self, count)) return err;
			return _msgpack_push(self, block_from(v));
		}
		else if (v.count <= UINT32_MAX)
		{
			uint8_t prefix = 0xdb;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)v.count;
			if (auto err = _msgpack_push(self, count)) return err;
			return _msgpack_push(self, block_from(v));
		}
		else
		{
			mn_unreachable();
			return errf("strings with count larger than 32 bit is not supported");
		}
	}

	inline static Err
	msgpack(Msgpack_Writer& self, const char* v)
	{
		return msgpack(self, str_lit(v));
	}

	inline static Err
	msgpack(Msgpack_Writer& self, Block v)
	{
		if (v.size <= UINT8_MAX)
		{
			uint8_t prefix = 0xc4;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)v.size;
			if (auto err = _msgpack_push(self, count)) return err;
			return _msgpack_push(self, v);
		}
		else if (v.size <= UINT16_MAX)
		{
			uint8_t prefix = 0xc5;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)v.size;
			if (auto err = _msgpack_push(self, count)) return err;
			return _msgpack_push(self, v);
		}
		else if (v.size <= UINT32_MAX)
		{
			uint8_t prefix = 0xc6;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)v.size;
			if (auto err = _msgpack_push(self, count)) return err;
			return _msgpack_push(self, v);
		}
		else
		{
			mn_unreachable();
			return errf("binary with count larger than 32 bit is not supported");
		}
	}

	template<typename T>
	inline static Err
	msgpack(Msgpack_Writer& self, const Buf<T>& v)
	{
		if (v.count <= 15)
		{
			uint8_t prefix = (uint8_t)v.count;
			prefix |= 0x90;
			if (auto err = _msgpack_push(self, prefix)) return err;
		}
		else if (v.count <= UINT16_MAX)
		{
			uint8_t prefix = 0xdc;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)v.count;
			if (auto err = _msgpack_push(self, count)) return err;
		}
		else if (v.count <= UINT32_MAX)
		{
			uint8_t prefix = 0xdd;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)v.count;
			if (auto err = _msgpack_push(self, count)) return err;
		}
		else
		{
			mn_unreachable();
			return errf("array with count larger than 32 bit is not supported");
		}

		for (const auto& a: v)
			if (auto err = msgpack(self, a))
				return err;
		return {};
	}

	template<typename TKey, typename TValue, typename THash>
	inline static Err
	msgpack(Msgpack_Writer& self, const Map<TKey, TValue, THash>& v)
	{
		if (v.count <= 15)
		{
			uint8_t prefix = (uint8_t)v.count;
			prefix |= 0x80;
			if (auto err = _msgpack_push(self, prefix)) return err;
		}
		else if (v.count <= UINT16_MAX)
		{
			uint8_t prefix = 0xde;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)v.count;
			if (auto err = _msgpack_push(self, count)) return err;
		}
		else if (v.count <= UINT32_MAX)
		{
			uint8_t prefix = 0xdf;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)v.count;
			if (auto err = _msgpack_push(self, count)) return err;
		}
		else
		{
			mn_unreachable();
			return errf("map with count larger than 32 bit is not supported");
		}

		for (const auto& a: v)
		{
			if (auto err = msgpack(self, a.key)) return err;
			if (auto err = msgpack(self, a.value)) return err;
		}
		return {};
	}

	struct Msgpack_Reader
	{
		Stream stream;
	};

	inline static Err
	_msgpack_pop(Msgpack_Reader& self, Block v)
	{
		auto [size, err] = stream_copy(v, self.stream);
		if (err)
			return errf("failed to read into memory stream, {}", io_error_message(err));
		if (size != v.size)
			return errf("failed to read {} bytes, only {} was read", v.size, size);
		return {};
	}

	template<typename T>
	inline static Err
	_msgpack_pop(Msgpack_Reader& self, T& v)
	{
		return _msgpack_pop(self, Block{&v, sizeof(v)});
	}

	inline static Msgpack_Reader
	msgpack_reader_new(Stream stream)
	{
		Msgpack_Reader self{};
		self.stream = stream;
		return self;
	}

	inline static Err
	msgpack(Msgpack_Reader& self, bool& res)
	{
		uint8_t rep{};
		if (auto err = _msgpack_pop(self, rep)) return err;
		if (rep == 0xc3)
			res = true;
		else if (rep == 0xc2)
			res = false;
		else
			return errf("invalid bool value {}", rep);
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, uint64_t& res)
	{
		uint8_t prefix{};
		if (auto err = _msgpack_pop(self, prefix)) return err;

		if (prefix <= 0x7f)
		{
			res = prefix;
		}
		else if (prefix == 0xcc)
		{
			uint8_t value{};
			if (auto err = _msgpack_pop(self, value)) return err;
			res = value;
		}
		else if (prefix == 0xcd)
		{
			uint16_t value{};
			if (auto err = _msgpack_pop(self, value)) return err;
			res = value;
		}
		else if (prefix == 0xce)
		{
			uint32_t value{};
			if (auto err = _msgpack_pop(self, value)) return err;
			res = value;
		}
		else if (prefix == 0xcf)
		{
			uint64_t value{};
			if (auto err = _msgpack_pop(self, value)) return err;
			res = value;
		}
		else
		{
			return errf("invalid uint value '{}'", prefix);
		}
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, uint8_t& res)
	{
		uint64_t value{};
		if (auto err = msgpack(self, value)) return err;
		if (value > UINT8_MAX)
			return errf("uint8 overflow, value is '{}'", value);
		res = (uint8_t)value;
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, uint16_t& res)
	{
		uint64_t value{};
		if (auto err = msgpack(self, value)) return err;
		if (value > UINT16_MAX)
			return errf("uint16 overflow, value is '{}'", value);
		res = (uint16_t)value;
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, uint32_t& res)
	{
		uint64_t value{};
		if (auto err = msgpack(self, value)) return err;
		if (value > UINT32_MAX)
			return errf("uint32 overflow, value is '{}'", value);
		res = (uint32_t)value;
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, int64_t& res)
	{
		uint8_t prefix{};
		if (auto err = _msgpack_pop(self, prefix)) return err;

		if (prefix <= 0x7f || prefix >= 0xE0)
		{
			res = (int64_t)prefix;
		}
		else if (prefix == 0xd0)
		{
			int8_t value{};
			if (auto err = _msgpack_pop(self, value)) return err;
			res = value;
		}
		else if (prefix == 0xd1)
		{
			int16_t value{};
			if (auto err = _msgpack_pop(self, value)) return err;
			res = value;
		}
		else if (prefix == 0xd2)
		{
			int32_t value{};
			if (auto err = _msgpack_pop(self, value)) return err;
			res = value;
		}
		else if (prefix == 0xd3)
		{
			int64_t value{};
			if (auto err = _msgpack_pop(self, value)) return err;
			res = value;
		}
		else
		{
			return errf("invalid int value '{}'", prefix);
		}
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, int8_t& res)
	{
		int64_t value{};
		if (auto err = msgpack(self, value)) return err;
		if (value > INT8_MAX)
			return errf("int8 overflow, value is '{}'", value);
		if (value < INT8_MIN)
			return errf("int8 underflow, value is '{}'", value);
		res = (int8_t)value;
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, int16_t& res)
	{
		int64_t value{};
		if (auto err = msgpack(self, value)) return err;
		if (value > INT16_MAX)
			return errf("int16 overflow, value is '{}'", value);
		if (value < INT16_MIN)
			return errf("int16 underflow, value is '{}'", value);
		res = (int16_t)value;
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, int32_t& res)
	{
		int64_t value{};
		if (auto err = msgpack(self, value)) return err;
		if (value > INT32_MAX)
			return errf("int32 overflow, value is '{}'", value);
		if (value < INT32_MIN)
			return errf("int32 underflow, value is '{}'", value);
		res = (int32_t)value;
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, float& res)
	{
		uint8_t prefix{};
		if (auto err = _msgpack_pop(self, prefix)) return err;

		if (prefix != 0xca)
			return errf("invalid float prefix '{}'", prefix);

		float value{};
		if (auto err = _msgpack_pop(self, value)) return err;
		res = value;
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, double& res)
	{
		uint8_t prefix{};
		if (auto err = _msgpack_pop(self, prefix)) return err;

		if (prefix != 0xcb)
			return errf("invalid double prefix '{}'", prefix);

		double value{};
		if (auto err = _msgpack_pop(self, value)) return err;
		res = value;
		return {};
	}

	inline static Err
	msgpack(Msgpack_Reader& self, Str& res)
	{
		uint8_t prefix{};
		if (auto err = _msgpack_pop(self, prefix)) return err;

		if (prefix >= 0xa0 && prefix <= 0xbf)
		{
			auto count = prefix & 0x1f;
			str_resize(res, count);
			return _msgpack_pop(self, block_from(res));
		}
		else if (prefix == 0xd9)
		{
			uint8_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;
			str_resize(res, count);
			return _msgpack_pop(self, block_from(res));
		}
		else if (prefix == 0xda)
		{
			uint16_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;
			str_resize(res, count);
			return _msgpack_pop(self, block_from(res));
		}
		else if (prefix == 0xdb)
		{
			uint32_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;
			str_resize(res, count);
			return _msgpack_pop(self, block_from(res));
		}
		else
		{
			return errf("invalid string prefix '{}'", prefix);
		}
	}

	inline static Err
	msgpack(Msgpack_Reader& self, Block& res, Allocator allocator = allocator_top())
	{
		uint8_t prefix{};
		if (auto err = _msgpack_pop(self, prefix)) return err;

		if (prefix == 0xc4)
		{
			uint8_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;

			auto value = alloc_from(allocator, count, alignof(char));
			mn_defer { free_from(allocator, value); };

			if (auto err = _msgpack_pop(self, value)) return err;
			res = xchg(value, {});
			return{};
		}
		else if (prefix == 0xc5)
		{
			uint16_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;

			auto value = alloc_from(allocator, count, alignof(char));
			mn_defer { free_from(allocator, value); };

			if (auto err = _msgpack_pop(self, value)) return err;
			res = xchg(value, {});
			return{};
		}
		else if (prefix == 0xc6)
		{
			uint32_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;

			auto value = alloc_from(allocator, count, alignof(char));
			mn_defer { free_from(allocator, value); };

			if (auto err = _msgpack_pop(self, value)) return err;
			res = xchg(value, {});
			return{};
		}
		else
		{
			return errf("invalid binary prefix '{}'", prefix);
		}
	}

	template<typename T>
	inline static Err
	msgpack(Msgpack_Reader& self, Buf<T>& res)
	{
		uint8_t prefix{};
		if (auto err = _msgpack_pop(self, prefix)) return err;

		if (prefix >= 0x90 && prefix <= 0x9f)
		{
			auto count = prefix & 0xf;
			buf_reserve(res, count);
		}
		else if (prefix == 0xdc)
		{
			uint16_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;
			buf_reserve(res, count);
		}
		else if (prefix == 0xdd)
		{
			uint32_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;
			buf_reserve(res, count);
		}
		else
		{
			return errf("invalid array prefix '{}'", prefix);
		}

		for (size_t i = 0; i < res.cap; ++i)
		{
			T v{};
			if (auto err = msgpack(self, v)) return err;
			buf_push(res, v);
		}
		return {};
	}

	template<typename TKey, typename TValue, typename THash>
	inline static Err
	msgpack(Msgpack_Reader& self, Map<TKey, TValue, THash>& res)
	{
		uint8_t prefix{};
		if (auto err = _msgpack_pop(self, prefix)) return err;

		size_t map_count = 0;
		if (prefix >= 0x80 && prefix <= 0x8f)
		{
			map_count = prefix & 0xf;
		}
		else if (prefix == 0xdc)
		{
			uint16_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;
			map_count = count;
		}
		else if (prefix == 0xdd)
		{
			uint32_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;
			map_count = count;
		}
		else
		{
			return errf("invalid map prefix '{}'", prefix);
		}

		map_reserve(res, map_count);
		for (size_t i = 0; i < map_count; ++i)
		{
			TKey key{};
			if (auto err = msgpack(self, key)) return err;

			TValue value{};
			if (auto err = msgpack(self, value)) return err;

			map_insert(res, key, value);
		}
		return {};
	}

	// struct helper code
	struct Msgpack_Field
	{
		mn::Str _name;
		const void* _value;
		Err (*_write)(Msgpack_Writer&, const void*);
		Err (*_read)(Msgpack_Reader&, void*);

		template<typename T>
		Msgpack_Field(const char* name, T* value)
		{
			_name = str_lit(name);
			_value = value;
			_write = +[](Msgpack_Writer& self, const void* ptr) -> Err {
				auto value = (const T*)ptr;
				return msgpack(self, *value);
			};
			_read = +[](Msgpack_Reader& self, void* ptr) -> Err {
				auto value = (std::remove_const_t<T>*)ptr;
				return msgpack(self, *value);
			};
		}
	};

	inline static Err
	msgpack_struct(Msgpack_Writer& self, std::initializer_list<Msgpack_Field> fields)
	{
		auto fields_count = fields.size();
		if (fields_count <= 15)
		{
			uint8_t prefix = (uint8_t)fields_count;
			prefix |= 0x80;
			if (auto err = _msgpack_push(self, prefix)) return err;
		}
		else if (fields_count <= UINT16_MAX)
		{
			uint8_t prefix = 0xde;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)fields_count;
			if (auto err = _msgpack_push(self, count)) return err;
		}
		else if (fields_count <= UINT32_MAX)
		{
			uint8_t prefix = 0xdf;
			if (auto err = _msgpack_push(self, prefix)) return err;
			uint8_t count = (uint8_t)fields_count;
			if (auto err = _msgpack_push(self, count)) return err;
		}
		else
		{
			mn_unreachable();
			return errf("map with count larger than 32 bit is not supported");
		}

		for (const auto& f: fields)
		{
			if (auto err = msgpack(self, f._name)) return err;
			if (auto err = f._write(self, f._value)) return err;
		}
		return {};
	}

	inline static Err
	msgpack_struct(Msgpack_Reader& self, std::initializer_list<Msgpack_Field> fields)
	{
		uint8_t prefix{};
		if (auto err = _msgpack_pop(self, prefix)) return err;

		size_t fields_count = 0;
		if (prefix >= 0x80 && prefix <= 0x8f)
		{
			fields_count = prefix & 0xf;
		}
		else if (prefix == 0xdc)
		{
			uint16_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;
			fields_count = count;
		}
		else if (prefix == 0xdd)
		{
			uint32_t count{};
			if (auto err = _msgpack_pop(self, count)) return err;
			fields_count = count;
		}
		else
		{
			return errf("invalid map prefix '{}'", prefix);
		}

		Str field_name{};
		mn_defer { str_free(field_name); };

		size_t required_fields = fields.size();
		for (size_t i = 0; i < fields_count; ++i)
		{
			str_clear(field_name);
			if (auto err = msgpack(self, field_name)) return err;

			for (auto& f: fields)
			{
				if (f._name == field_name)
				{
					if (auto err = f._read(self, (void*)f._value)) return err;
					--required_fields;
					break;
				}
			}
		}

		if (required_fields != 0)
			return errf("missing struct fields");

		return {};
	}

	// helper encode/decode functions
	template<typename T>
	inline static Result<Str>
	msgpack_encode(const T& value)
	{
		auto writer = msgpack_writer_new();
		mn_defer { msgpack_writer_free(writer); };

		if (auto err = msgpack(writer, value)) return err;

		return memory_stream_str(writer.stream);
	}

	template<typename T>
	inline static Err
	msgpack_decode(Block bytes, T& value)
	{
		auto stream = block_stream_wrap(bytes);
		auto reader = msgpack_reader_new(&stream);
		return msgpack(reader, value);
	}

	template<typename T>
	inline static Err
	msgpack_decode(Str bytes, T& value)
	{
		return msgpack_decode(block_from(bytes), value);
	}
}