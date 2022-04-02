#include "mn/Path.h"

#import <Foundation/Foundation.h>

namespace mn
{
	Str
	folder_tmp(Allocator allocator)
	{
		auto home = NSHomeDirectory();
		auto res = str_from_c([home UTF8String], allocator);
		[home release];
		return res;
	}

	Str
	folder_config(Allocator allocator)
	{
		// NOTE(mahmoud adas): we can't use `~` becausee macOS doesn't expand it on ::mkdir
		auto temp = NSTemporaryDirectory();
		auto res = path_join(str_with_allocator(allocator), [temp UTF8String], "Library", "Preferences");
		[temp release];
		return res;
	}
}