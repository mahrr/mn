#pragma once

#include "mn/File.h"

#include <pthread.h>

namespace mn
{
	struct IMutex
	{
		pthread_mutex_t handle;
		const char* name;
		const Source_Location* srcloc;
		void* profile_user_data;
	};
}