#pragma once

#include "mn/File.h"

#include <pthread.h>

namespace mn
{
	struct IMutex_RW
	{
		pthread_rwlock_t lock;
		const char* name;
		const Source_Location* srcloc;
		void* profile_user_data;
	};
}
