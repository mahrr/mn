#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace mn
{
	struct IWaitgroup
	{
		int count;
		CRITICAL_SECTION cs;
		CONDITION_VARIABLE cv;
	};
}