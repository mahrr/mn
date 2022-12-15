#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace mn
{
	struct ICond_Var
	{
		CONDITION_VARIABLE cv;
	};
}