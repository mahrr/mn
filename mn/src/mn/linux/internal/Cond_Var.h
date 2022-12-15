#pragma once

#include "mn/File.h"

#include <pthread.h>

namespace mn
{
	struct ICond_Var
	{
		pthread_cond_t cv;
	};
}
