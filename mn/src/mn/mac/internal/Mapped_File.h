#pragma once

#include "mn/File.h"

namespace mn
{
	struct IMapped_File
	{
		Mapped_File file_view;
		// if set this means that the mapped file owns it
		File mn_file_handle;
	};
}