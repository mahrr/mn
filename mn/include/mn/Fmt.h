#pragma once

#include <fmt/core.h>

#include <string_view>
#include <iterator>

#include "mn/Str.h"
#include "mn/Buf.h"
#include "mn/Map.h"
#include "mn/File.h"
#include "mn/Result.h"

namespace fmt
{
	template<>
	struct formatter<mn::Str> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const mn::Str &str, FormatContext &ctx) {
			if (str.count == 0)
				return ctx.out();
			return format_to(ctx.out(), "{}", std::string_view{str.ptr, str.count});
		}
	};

	template<typename T>
	struct formatter<mn::Buf<T>> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const mn::Buf<T> &buf, FormatContext &ctx) {
			format_to(ctx.out(), "[{}]{{", buf.count);
			for(size_t i = 0; i < buf.count; ++i)
			{
				if(i != 0)
					format_to(ctx.out(), ", ");
				format_to(ctx.out(), "{}: {}", i, buf[i]);
			}
			format_to(ctx.out(), " }}");
			return ctx.out();
		}
	};

	template<typename T, typename THash>
	struct formatter<mn::Set<T, THash>> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const mn::Set<T, THash> &set, FormatContext &ctx) {
			format_to(ctx.out(), "[{}]{{ ", set.count);
			size_t i = 0;
			for(const auto& value: set)
			{
				if(i != 0)
					format_to(ctx.out(), ", ");
				format_to(ctx.out(), "{}", value);
				++i;
			}
			format_to(ctx.out(), " }}");
			return ctx.out();
		}
	};

	template<typename TKey, typename TValue, typename THash>
	struct formatter<mn::Map<TKey, TValue, THash>> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const mn::Map<TKey, TValue, THash> &map, FormatContext &ctx) {
			format_to(ctx.out(), "[{}]{{ ", map.count);
			size_t i = 0;
			for(const auto& [key, value]: map)
			{
				if(i != 0)
					format_to(ctx.out(), ", ");
				format_to(ctx.out(), "{}: {}", key, value);
				++i;
			}
			format_to(ctx.out(), " }}");
			return ctx.out();
		}
	};

	template<>
	struct formatter<mn::Err>
	{
		template<typename ParseContext>
		constexpr auto
		parse(ParseContext& ctx)
		{
			return ctx.begin();
		}

		template<typename FormatContext>
		auto
		format(const mn::Err& err, FormatContext& ctx)
		{
			return format_to(ctx.out(), "{}", err.msg);
		}
	};
}

namespace mn
{
	struct _Str_Back_Insert_Iterator
	{
		using iterator_category = std::output_iterator_tag;
		Str* str;

		_Str_Back_Insert_Iterator(Str& s)
			: str(&s)
		{}

		_Str_Back_Insert_Iterator&
		operator=(char v)
		{
			str_push(*str, v);
			return *this;
		}

		_Str_Back_Insert_Iterator&
		operator*()
		{
			return *this;
		}

		_Str_Back_Insert_Iterator&
		operator++()
		{
			return *this;
		}

		_Str_Back_Insert_Iterator
		operator++(int)
		{
			return *this;
		}
	};

	struct _Stream_Back_Insert_Iterator
	{
		using iterator_category = std::output_iterator_tag;
		Stream stream;
		size_t written_bytes;

		_Stream_Back_Insert_Iterator(Stream s)
			: stream(s),
			  written_bytes(0)
		{}

		_Stream_Back_Insert_Iterator&
		operator=(char v)
		{
			written_bytes += stream_write(stream, Block{ &v, sizeof(v) });
			return *this;
		}

		_Stream_Back_Insert_Iterator&
		operator*()
		{
			return *this;
		}

		_Stream_Back_Insert_Iterator&
		operator++()
		{
			return *this;
		}

		_Stream_Back_Insert_Iterator
		operator++(int)
		{
			return *this;
		}
	};
}

namespace std
{
	template<>
	struct iterator_traits<mn::_Str_Back_Insert_Iterator>
	{
		typedef ptrdiff_t difference_type;
		typedef char value_type;
		typedef char* pointer;
		typedef char& reference;
		typedef std::output_iterator_tag iterator_category;
	};

	template<>
	struct iterator_traits<mn::_Stream_Back_Insert_Iterator>
	{
		typedef ptrdiff_t difference_type;
		typedef char value_type;
		typedef char* pointer;
		typedef char& reference;
		typedef std::output_iterator_tag iterator_category;
	};
}

namespace mn
{
	// appends the formatted string to the end of the given out string, you should the returned value back into the given
	// string
	template<typename ... Args>
	[[nodiscard]] inline static Str
	strf(Str out, const char* format_str, const Args& ... args)
	{
		fmt::format_to(_Str_Back_Insert_Iterator(out), format_str, args...);
		return out;
	}

	// creates a new string with the given allocator containing the formatted string
	template<typename ... Args>
	[[nodiscard]] inline static Str
	strf(Allocator allocator, const char* format_str, const Args& ... args)
	{
		return strf(str_with_allocator(allocator), format_str, args...);
	}

	// creates a new string using the top/default allocator containing the formatted string
	template<typename ... Args>
	[[nodiscard]] inline static Str
	strf(const char* format_str, const Args& ... args)
	{
		return strf(str_new(), format_str, args...);
	}

	// creates a new Err with the given formatted error message
	[[nodiscard]] inline static Err
	errf(const char* str)
	{
		return Err{ str_lit(str) };
	}

	// creates a new Err with the given formatted error message
	template<typename ... Args>
	[[nodiscard]] inline static Err
	errf(const char* format_str, const Args& ... args)
	{
		return Err{ strf(str_new(), format_str, args...) };
	}

	// creates a new temporary string using the tmp allocator containing the formatted string
	template<typename ... Args>
	inline static Str
	str_tmpf(const char* format_str, const Args& ... args)
	{
		return strf(str_tmp(), format_str, args...);
	}

	// prints the formatted string to the given stream
	template<typename ... Args>
	inline static size_t
	print_to(Stream stream, const char* format_str, const Args& ... args)
	{
		_Stream_Back_Insert_Iterator it{ stream };
		fmt::format_to(it, format_str, args...);
		return it.written_bytes;
	}

	// prints the formatted string to the standard output stream
	template<typename ... Args>
	inline static size_t
	print(const char* format_str, const Args& ... args)
	{
		return print_to(file_stdout(), format_str, args...);
	}

	// prints the formatted string to the standard error stream
	template<typename ... Args>
	inline static size_t
	printerr(const char* format_str, const Args& ... args)
	{
		return print_to(file_stderr(), format_str, args...);
	}

	// joins the given strings with the delimiter and appends the result to the given str
	inline static Str
	str_join(Str str, const Str* begin, const Str* end, const Str& delimiter)
	{
		if (begin != end)
		{
			str = strf(str, "{}", *begin);
			for (auto it = begin + 1; it != end; ++it)
				str = strf(str, "{}{}", delimiter, *it);
		}
		return str;
	}

	// joins the given strings with the delimiter and appends the result to the given str
	inline static Str
	str_join(Allocator allocator, const Str* begin, const Str* end, const Str& delimiter)
	{
		return str_join(str_with_allocator(allocator), begin, end, delimiter);
	}

	// joins the given strings with the delimiter and appends the result to the given str
	inline static Str
	str_join(const Str* begin, const Str* end, const Str& delimiter)
	{
		return str_join(str_new(), begin, end, delimiter);
	}
}