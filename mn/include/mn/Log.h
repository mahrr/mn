#pragma once

#include "mn/Context.h"
#include "mn/Fmt.h"

namespace mn
{
	// logs a message with debug level, it will be disabled in release mode
	template<typename... TArgs>
	inline static void
	log_debug([[maybe_unused]] const char* fmt, [[maybe_unused]] TArgs&&... args)
	{
		#ifdef DEBUG
		auto msg = str_tmpf(fmt, args...);
		_log_debug_str(msg.ptr);
		#endif
	}

	// logs a message with info level
	template<typename... TArgs>
	inline static void
	log_info(const char* fmt, TArgs&&... args)
	{
		auto msg = str_tmpf(fmt, args...);
		_log_info_str(msg.ptr);
	}

	// logs a message with warning level
	template<typename... TArgs>
	inline static void
	log_warning(const char* fmt, TArgs&&... args)
	{
		auto msg = str_tmpf(fmt, args...);
		_log_warning_str(msg.ptr);
	}

	// logs a message with error level
	template<typename... TArgs>
	inline static void
	log_error(const char* fmt, TArgs&&... args)
	{
		auto msg = str_tmpf(fmt, args...);
		_log_error_str(msg.ptr);
	}

	// logs a message with critical level, and terminates the program
	template<typename... TArgs>
	[[noreturn]] inline static void
	log_critical(const char* fmt, TArgs&&... args)
	{
		auto msg = str_tmpf(fmt, args...);
		_log_critical_str(msg.ptr);
		::abort();
	}

	// checks the given boolean flag if it's false it will log the given message with critical level and exists the program
	template<typename... TArgs>
	inline static void
	log_ensure(bool expr, const char* fmt, TArgs&&... args)
	{
		if (expr == false)
			log_critical(fmt, args...);
	}
}