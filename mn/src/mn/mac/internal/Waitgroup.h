#pragma once

#include "mn/File.h"

#include <pthread.h>

namespace mn
{
	struct IWaitgroup
	{
		int count;
		pthread_mutex_t mtx;
		pthread_cond_t cv;
	};
}
