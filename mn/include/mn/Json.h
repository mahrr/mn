#pragma once

#include "mn/Exports.h"
#include "mn/Str.h"
#include "mn/Buf.h"
#include "mn/Map.h"
#include "mn/Result.h"
#include "mn/Fmt.h"
#include "mn/Assert.h"

namespace mn::json
{
	// represents a json value
	struct Value
	{
		enum KIND: uint8_t
		{
			KIND_NULL,
			KIND_BOOL,
			KIND_NUMBER,
			KIND_STRING,
			KIND_ARRAY,
			KIND_OBJECT,
		};

		KIND kind;
		union
		{
			bool as_bool;
			float as_number;
			Str* as_string;
			Buf<Value>* as_array;
			Map<Str, Value>* as_object;
		};
	};

	// creates a new json value from a boolean
	inline static Value
	value_bool_new(bool v)
	{
		Value self{};
		self.kind = Value::KIND_BOOL;
		self.as_bool = v;
		return self;
	}

	// creates a new json value from a number
	inline static Value
	value_number_new(float v)
	{
		Value self{};
		self.kind = Value::KIND_NUMBER;
		self.as_number = v;
		return self;
	}

	// creates a new json value from a string
	inline static Value
	value_string_new(const Str& v)
	{
		Value self{};
		self.kind = Value::KIND_STRING;
		self.as_string = alloc<Str>();
		*self.as_string = clone(v);
		return self;
	}

	// creates a new json value from a string
	inline static Value
	value_string_new(const char* v)
	{
		Value self{};
		self.kind = Value::KIND_STRING;
		self.as_string = alloc<Str>();
		*self.as_string = str_from_c(v);
		return self;
	}

	// creates a new json array
	inline static Value
	value_array_new()
	{
		Value self{};
		self.kind = Value::KIND_ARRAY;
		self.as_array = alloc_zerod<Buf<Value>>();
		return self;
	}

	// creates a new json object
	inline static Value
	value_object_new()
	{
		Value self{};
		self.kind = Value::KIND_OBJECT;
		self.as_object = alloc_zerod<Map<Str, Value>>();
		return self;
	}

	// frees the given json value
	inline static void
	value_free(Value& self)
	{
		switch(self.kind)
		{
		case Value::KIND_NULL:
		case Value::KIND_BOOL:
		case Value::KIND_NUMBER:
			break;
		case Value::KIND_STRING:
			str_free(*self.as_string);
			free(self.as_string);
			break;
		case Value::KIND_ARRAY:
			destruct(*self.as_array);
			free(self.as_array);
			break;
		case Value::KIND_OBJECT:
			destruct(*self.as_object);
			free(self.as_object);
			break;
		default:
			mn_unreachable();
			break;
		}
	}

	// destruct overload for value free
	inline static void
	destruct(Value& self)
	{
		value_free(self);
	}

	// returns the json value in the given array at the given index
	inline static const Value&
	value_array_at(const Value& self, size_t index)
	{
		return *(self.as_array->ptr + index);
	}

	// returns the json value in the given array at the given index
	inline static Value&
	value_array_at(Value& self, size_t index)
	{
		return *(self.as_array->ptr + index);
	}

	// pushes a new value into the given json array
	inline static void
	value_array_push(Value& self, Value v)
	{
		buf_push(*self.as_array, v);
	}

	// iterates over the given json array
	inline static Buf<Value>&
	value_array_iter(Value& self)
	{
		return *self.as_array;
	}

	// iterates over the given json array
	inline static const Buf<Value>&
	value_array_iter(const Value& self)
	{
		return *self.as_array;
	}

	// searches for a key inside the given json object, returns nullptr if the key doesn't exist
	inline static const Value*
	value_object_lookup(const Value& self, const Str& key)
	{
		if (auto it = map_lookup(*self.as_object, key))
			return &it->value;
		return nullptr;
	}

	// searches for a key inside the given json object, returns nullptr if the key doesn't exist
	inline static Value*
	value_object_lookup(Value& self, const Str& key)
	{
		if (auto it = map_lookup(*self.as_object, key))
			return &it->value;
		return nullptr;
	}

	// searches for a key inside the given json object, returns nullptr if the key doesn't exist
	inline static const Value*
	value_object_lookup(const Value& self, const char* key)
	{
		if (auto it = map_lookup(*self.as_object, str_lit(key)))
			return &it->value;
		return nullptr;
	}

	// searches for a key inside the given json object, returns nullptr if the key doesn't exist
	inline static Value*
	value_object_lookup(Value& self, const char* key)
	{
		if (auto it = map_lookup(*self.as_object, str_lit(key)))
			return &it->value;
		return nullptr;
	}

	// inserts a new key value pair into the given json value
	inline static void
	value_object_insert(Value& self, const Str& key, Value v)
	{
		if (auto it = map_lookup(*self.as_object, key))
		{
			value_free(it->value);
			it->value = v;
		}
		else
		{
			map_insert(*self.as_object, clone(key), v);
		}
	}

	// inserts a new key value pair into the given json value
	inline static void
	value_object_insert(Value& self, const char* key, Value v)
	{
		map_insert(*self.as_object, str_from_c(key), v);
	}

	// iterates over the given json object
	inline static Map<Str, Value>&
	value_object_iter(Value& self)
	{
		return *self.as_object;
	}

	// iterates over the given json object
	inline static const Map<Str, Value>&
	value_object_iter(const Value& self)
	{
		return *self.as_object;
	}

	// tries to parse json value from the encoded string
	MN_EXPORT Result<Value>
	parse(const Str& content);

	// tries to parse json value from the encoded string
	inline static Result<Value>
	parse(const char* content)
	{
		return parse(str_lit(content));
	}

	// clones the given json value
	inline static Value
	value_clone(const Value& other)
	{
		switch (other.kind)
		{
		case Value::KIND_NULL:
		case Value::KIND_BOOL:
		case Value::KIND_NUMBER:
			return other;
		case Value::KIND_STRING:
			return value_string_new(*other.as_string);
		case Value::KIND_ARRAY:
		{
			auto self = value_array_new();
			buf_reserve(*self.as_array, other.as_array->count);
			for (auto v: value_array_iter(other))
				buf_push(*self.as_array, value_clone(v));
			return self;
		}
		case Value::KIND_OBJECT:
		{
			auto self = value_object_new();
			map_reserve(*self.as_object, other.as_object->count);
			for (const auto& [k, v]: value_object_iter(other))
				map_insert(*self.as_object, clone(k), value_clone(v));
			return self;
		}
		default:
			mn_unreachable();
			return Value{};
		}
	}

	// clone overload for json value
	inline static Value
	clone(const Value& other)
	{
		return value_clone(other);
	}

	template<typename T>
	inline static Err
	unpack(Value v, T* self, Value::KIND kind);

	template<typename T>
	inline static Err
	_extract_string(Value, T*)
	{
		return mn::errf("the provided element pointer is not of type const char* or Str");
	}

	inline static Err
	_extract_string(Value v, const char** self)
	{
		if (self)
			*self = v.as_string->ptr;
		return Err{};
	}

	inline static Err
	_extract_string(Value v, Str* self)
	{
		if (self)
		{
			str_clear(*self);
			str_push(*self, *v.as_string);
		}
		return Err{};
	}

	template<typename T>
	inline static Err
	_extract_buf_from_array(Value, T*)
	{
		return mn::errf("the provided element pointer is not of type Buf<T>");
	}

	template<typename T>
	inline static Err
	_extract_buf_from_array(Value v, Buf<T>* buf)
	{
		auto count = v.as_array->count;
		if (count == 0)
		{
			buf_resize(*buf, 0);
			return Err{};
		}

		auto first_type = (*v.as_array)[0].kind;
		for (size_t i = 1; i < count; ++i)
		{
			if (first_type != (*v.as_array)[i].kind)
				return mn::errf("can't read non uniform dyno array into a uniform Buf<T>");
		}

		buf_resize(*buf, count);
		for (size_t i = 0; i < count; ++i)
		{
			(*buf)[i] = {0};
			auto err = unpack((*v.as_array)[i], buf->ptr + i, first_type);
			if (err)
				return err;
		}
		return Err{};
	}

	template<typename T>
	inline static Err
	unpack(Value v, T* self, Value::KIND kind)
	{
		if (v.kind == Value::KIND_NULL)
			return mn::errf("Value is null");

		if (v.kind != kind)
			return mn::errf("mismatched value type");

		auto err = Err{};
		switch (kind)
		{
		case Value::KIND_BOOL:
			if constexpr (std::is_same_v<bool, T>)
			{
				if (self)
					*self = T(v.as_bool);
			}
			else
			{
				err = mn::errf("the provided element pointer is not of type bool");
			}
			break;
		case Value::KIND_NUMBER:
			if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>)
			{
				if (self)
				{
					*self = T(v.as_number);
					if ((double)*self != (double)v.as_number)
					{
						err = mn::errf("loss of percision while unpacking a value");
					}
				}
			}
			else
			{
				err = mn::errf("the provided element pointer is not of signed integral type");
			}
			break;
		case Value::KIND_STRING:
			err = _extract_string(v, self);
			break;
		case Value::KIND_ARRAY:
			err = _extract_buf_from_array(v, self);
			break;
		default:
			err = mn::errf("unsupported unpack type");
			break;
		}
		return err;
	}

	template<typename T>
	inline static constexpr bool
	_is_buf(T*)
	{
		return false;
	}

	template<typename T>
	inline static constexpr bool
	_is_buf(Buf<T>*)
	{
		return true;
	}

	template<typename T>
	inline static Err
	unpack(Value v, T* self)
	{
		if constexpr (std::is_same_v<T, bool>)
			return unpack(v, self, Value::KIND_BOOL);
		else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>)
			return unpack(v, self, Value::KIND_NUMBER);
		else if constexpr (std::is_same_v<T, Str> || std::is_same_v<T, const char *>)
			return unpack(v, self, Value::KIND_STRING);
		else if constexpr (_is_buf<T>)
			return unpack(v, self, v.kind);
		else
			static_assert(sizeof(T) == 0, "unsupported primitive type");
	}

	struct Struct_Element
	{
		void *ptr;
		Err (*unpack)(Value, void*, Value::KIND);
		Value::KIND kind;
		const char* name;

		Struct_Element(const char* field_name, Value::KIND field_kind)
			: ptr(nullptr), unpack(nullptr), kind(field_kind), name(field_name)
		{
		}

		template<typename T>
		Struct_Element(T* data, const char* field_name, Value::KIND field_kind)
			: ptr(data), unpack(nullptr), kind(field_kind), name(field_name)
		{
			unpack = [](Value v, void *ptr, Value::KIND type) { return mn::json::unpack<T>(v, (T *)ptr, type); };
		}

#define SMART_CONSTRUCTOR(ctype, valuetype)                                                                             \
	Struct_Element(ctype* data, const char* field_name) : ptr(data), unpack(nullptr), kind(valuetype), name(field_name) \
	{                                                                                                                  \
		unpack = [](Value v, void *ptr, Value::KIND type) { return mn::json::unpack<ctype>(v, (ctype *)ptr, type); };   \
	}

		SMART_CONSTRUCTOR(bool, Value::KIND_BOOL)
		SMART_CONSTRUCTOR(int8_t, Value::KIND_NUMBER)
		SMART_CONSTRUCTOR(int16_t, Value::KIND_NUMBER)
		SMART_CONSTRUCTOR(int32_t, Value::KIND_NUMBER)
		SMART_CONSTRUCTOR(int64_t, Value::KIND_NUMBER)
		SMART_CONSTRUCTOR(uint8_t, Value::KIND_NUMBER)
		SMART_CONSTRUCTOR(uint16_t, Value::KIND_NUMBER)
		SMART_CONSTRUCTOR(uint32_t, Value::KIND_NUMBER)
		SMART_CONSTRUCTOR(uint64_t, Value::KIND_NUMBER)
		#if OS_MACOS
		SMART_CONSTRUCTOR(size_t, Value::KIND_NUMBER)
		#endif
		SMART_CONSTRUCTOR(float, Value::KIND_NUMBER)
		SMART_CONSTRUCTOR(double, Value::KIND_NUMBER)
		SMART_CONSTRUCTOR(const char *, Value::KIND_STRING)
		SMART_CONSTRUCTOR(Str, Value::KIND_STRING)
#undef SMART_CONSTRUCTOR
	};

	// given a list of pointers to data and their json path, we unpack the data
	// into the given pointers and return error in case we don't find any
	// example usage:
	// if you have the following json value:
	// {"package": "sabre", "uniform": {"name": camera, "size": 16}}
	// you can unpack it like this
	// auto err = mn::json::unpack(value, {
	// 	{&package_name, "package"},
	// 	{&uniform_name, "uniform.name"},
	// 	{&uniform_size, "uniform.size"}
	// });
	inline static Err
	unpack(Value v, std::initializer_list<Struct_Element> elements)
	{
		if (v.kind == Value::KIND_NULL)
			return mn::errf("value is null");

		auto values = buf_with_allocator<Value>(memory::tmp());
		buf_resize(values, elements.size());

		auto elements_it = elements.begin();
		for (size_t i = 0; i < elements.size(); ++i)
		{
			const auto &e	 = *elements_it;
			auto path		 = str_split(e.name, ".", true);

			Value field = v;
			for (auto field_name : path)
			{
				if (field.kind != Value::KIND_OBJECT)
					return mn::errf("accessing a non-object value while traversing key '{}'", field_name);

				if (auto subfield = value_object_lookup(field, field_name))
					field = *subfield;
				else
					return mn::errf("struct doesn't have a '{}' field", e.name);
			}

			values[i] = field;
			++elements_it;
		}

		elements_it = elements.begin();
		for (size_t i = 0; i < elements.size(); ++i)
		{
			const auto &e = *elements_it;
			if (e.unpack)
			{
				if (auto err = e.unpack(values[i], e.ptr, e.kind); err)
					return err;
			}
			else
			{
				if (values[i].kind != e.kind)
					return mn::errf("dyno type mismatch in field '{}'", e.name);
			}
			++elements_it;
		}

		return Err{};
	}
}

namespace fmt
{
	template<>
	struct formatter<mn::json::Value> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const mn::json::Value &v, FormatContext &ctx) {
			switch(v.kind)
			{
			case mn::json::Value::KIND_NULL:
				format_to(ctx.out(), "null");
				break;
			case mn::json::Value::KIND_BOOL:
				format_to(ctx.out(), "{}", v.as_bool ? "true" : "false");
				break;
			case mn::json::Value::KIND_NUMBER:
				format_to(ctx.out(), "{}", v.as_number);
				break;
			case mn::json::Value::KIND_STRING:
				format_to(ctx.out(), "\"{}\"", *v.as_string);
				break;
			case mn::json::Value::KIND_ARRAY:
				format_to(ctx.out(), "[");
				for(size_t i = 0; i < v.as_array->count; ++i)
				{
					if (i != 0)
						format_to(ctx.out(), ", ");
					format_to(ctx.out(), "{}", (*v.as_array)[i]);
				}
				format_to(ctx.out(), "]");
				break;
			case mn::json::Value::KIND_OBJECT:
			{
				format_to(ctx.out(), "{{");
				size_t i = 0;
				for (const auto& [key, value]: *v.as_object)
				{
					if (i != 0)
						format_to(ctx.out(), ", ");
					format_to(ctx.out(), "\"{}\":{}", key, value);
					++i;
				}
				format_to(ctx.out(), "}}");
				break;
			}
			default:
				mn_unreachable();
				break;
			}
			return ctx.out();
		}
	};
}