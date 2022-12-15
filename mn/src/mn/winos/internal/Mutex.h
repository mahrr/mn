#pragma once

#include "mn/Base.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace mn
{
	struct IMutex
	{
		const Source_Location* srcloc;
		const char* name;
		CRITICAL_SECTION cs;
		void* profile_user_data;
	};
}