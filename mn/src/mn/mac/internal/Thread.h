#pragma once

#include "mn/File.h"

#include <pthread.h>

namespace mn
{
	struct IThread
	{
		pthread_t handle;
		Thread_Func func;
		void* user_data;
		const char* name;
	};
}
