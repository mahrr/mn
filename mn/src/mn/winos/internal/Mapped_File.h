#pragma once

#include "mn/File.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace mn
{
	struct IMapped_File
	{
		Mapped_File file_view;
		HANDLE file_map;
		// if set this means that the mapped file owns it
		File mn_file_handle;
	};
}