#pragma once

#include "mn/Thread.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace mn
{
	struct IThread
	{
		HANDLE handle;
		DWORD id;
		Thread_Func func;
		void* user_data;
		const char* name;
	};
}