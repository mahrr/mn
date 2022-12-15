#pragma once

#include "mn/Base.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace mn
{
	struct IMutex_RW
	{
		SRWLOCK lock;
		const char* name;
		const Source_Location* srcloc;
		void* profile_user_data;
	};
}