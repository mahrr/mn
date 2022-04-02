#pragma once

#include "mn/Exports.h"
#include "mn/Base.h"
#include "mn/Str.h"

namespace mn
{
	// a helper function which given the filename will load the content of it into the resulting string
	MN_EXPORT Str
	file_content_str(const char* filename, Allocator allocator = allocator_top());

	// a helper function which given the filename will load the content of it into the resulting string
	inline static Str
	file_content_str(const Str& filename, Allocator allocator = allocator_top())
	{
		return file_content_str(filename.ptr, allocator);
	}


	// converts from standard encoding (linux-like) to is specific encoding
	// you probably won't need to use this function because mn takes care of conversions internally
	// it might be of use if you're writing os specific code outside of mn
	MN_EXPORT Str
	path_os_encoding(const char* path, Allocator allocator = memory::tmp());

	// converts from standard encoding (linux-like) to is specific encoding
	// you probably won't need to use this function because mn takes care of conversions internally
	// it might be of use if you're writing os specific code outside of mn
	inline static Str
	path_os_encoding(const Str& path, Allocator allocator = memory::tmp())
	{
		return path_os_encoding(path.ptr, allocator);
	}

	// removes duplicate / and other sanitization stuff
	MN_EXPORT Str
	path_sanitize(Str path);

	// converts from os specific encoding to standard encoding (linux-like)
	MN_EXPORT Str
	path_normalize(Str path);

	inline static Str
	path_join(Str base)
	{
		return path_sanitize(base);
	}

	// joins multiple path fragments together in the path
	// base = "base_folder"
	// base = path_join(base, "my_folder1", "my_folder2", "my_file") = "base_folder/my_folder1/my_folder2/my_file"
	template<typename TFirst, typename ... TArgs>
	inline static Str
	path_join(Str base, TFirst&& first, TArgs&& ... args)
	{
		if (base.count > 0 && str_suffix(base, "/") == false)
			str_push(base, "/");
		str_push(base, std::forward<TFirst>(first));
		return path_join(base, std::forward<TArgs>(args)...);
	}

	// returns whether the path exists
	MN_EXPORT bool
	path_exists(const char* path);

	// returns whether the path exists
	inline static bool
	path_exists(const Str& path)
	{
		return path_exists(path.ptr);
	}

	// returns whether the given path is a folder
	MN_EXPORT bool
	path_is_folder(const char* path);

	// returns whether the given path is a folder
	inline static bool
	path_is_folder(const Str& path)
	{
		return path_is_folder(path.ptr);
	}

	// returns whether the given path is a file
	MN_EXPORT bool
	path_is_file(const char* path);

	// returns whether the given path is a file
	inline static bool
	path_is_file(const Str& path)
	{
		return path_is_file(path.ptr);
	}

	// returns the current working path of the process
	MN_EXPORT Str
	path_current(Allocator allocator = allocator_top());

	// changes the current working path of the process
	MN_EXPORT void
	path_current_change(const char* path);

	// changes the current working path of the process
	inline static void
	path_current_change(const Str& path)
	{
		path_current_change(path.ptr);
	}

	// returns the absolute path of the given relative path
	MN_EXPORT Str
	path_absolute(const char* path, Allocator allocator = allocator_top());

	// returns the absolute path of the given relative path
	inline static Str
	path_absolute(const Str& path, Allocator allocator = allocator_top())
	{
		return path_absolute(path.ptr, allocator);
	}

	// returns the directory of the given file path
	MN_EXPORT Str
	file_directory(const char* path, Allocator allocator = allocator_top());

	// returns the directory of the given file path
	inline static Str
	file_directory(const Str& path, Allocator allocator = allocator_top())
	{
		return file_directory(path.ptr, allocator);
	}

	// represents a file system entity file/folder
	struct Path_Entry
	{
		enum KIND
		{
			KIND_FILE,
			KIND_FOLDER
		};

		KIND kind;
		Str  name;
	};

	// frees the given path entry
	inline static void
	path_entry_free(Path_Entry& self)
	{
		str_free(self.name);
	}

	// destruct overload of path entry free
	inline static void
	destruct(Path_Entry& self)
	{
		path_entry_free(self);
	}

	// returns the names of the children files/folders of the given path
	MN_EXPORT Buf<Path_Entry>
	path_entries(const char* path, Allocator allocator = allocator_top());

	// returns the names of the children files/folders of the given path
	inline static Buf<Path_Entry>
	path_entries(const Str& path, Allocator allocator = allocator_top())
	{
		return path_entries(path.ptr, allocator);
	}

	// returns the absolute path of the executable
	MN_EXPORT Str
	path_executable(Allocator allocator = allocator_top());

	// returns the time of the last write to the given file
	// if path is not correct it will return 0
	MN_EXPORT int64_t
	file_last_write_time(const char* path);

	// returns the time of the last write to the given file
	// if path is not correct it will return 0
	inline static int64_t
	file_last_write_time(const mn::Str& path)
	{
		return file_last_write_time(path.ptr);
	}


	// copies a file from src to dst, and returns whether the operation is successful
	MN_EXPORT bool
	file_copy(const char* src, const char* dst);

	// copies a file from src to dst, and returns whether the operation is successful
	inline static bool
	file_copy(const Str& src, const char* dst)
	{
		return file_copy(src.ptr, dst);
	}

	// copies a file from src to dst, and returns whether the operation is successful
	inline static bool
	file_copy(const char* src, const Str& dst)
	{
		return file_copy(src, dst.ptr);
	}

	// copies a file from src to dst, and returns whether the operation is successful
	inline static bool
	file_copy(const Str& src, const Str& dst)
	{
		return file_copy(src.ptr, dst.ptr);
	}

	// removes a file, and returns whether the operation is successful
	MN_EXPORT bool
	file_remove(const char* path);

	// removes a file, and returns whether the operation is successful
	inline static bool
	file_remove(const Str& path)
	{
		return file_remove(path.ptr);
	}

	// moves a file from src to dst, and returns whether the operation is successful
	MN_EXPORT bool
	file_move(const char* src, const char* dst);

	// moves a file from src to dst, and returns whether the operation is successful
	inline static bool
	file_move(const Str& src, const char* dst)
	{
		return file_move(src.ptr, dst);
	}

	// moves a file from src to dst, and returns whether the operation is successful
	inline static bool
	file_move(const char* src, const Str& dst)
	{
		return file_move(src, dst.ptr);
	}

	// moves a file from src to dst, and returns whether the operation is successful
	inline static bool
	file_move(const Str& src, const Str& dst)
	{
		return file_move(src.ptr, dst.ptr);
	}

	// returns the file name part of the given path
	MN_EXPORT Str
	file_name(const Str& path, Allocator allocator = allocator_top());

	// returns the file name part of the given path
	inline static Str
	file_name(const char* path, Allocator allocator = allocator_top())
	{
		return file_name(str_lit(path), allocator);
	}

	// returns a new file path inside the temproray folder using the given base name and extension
	// file_tmp("base", "png"): /tmp/base_3248239476.png
	MN_EXPORT Str
	file_tmp(const Str& base, const Str& ext, Allocator allocator = allocator_top());

	// returns a new file path inside the temproray folder using the given base name and extension
	// file_tmp("base", "png"): /tmp/base_3248239476.png
	inline static Str
	file_tmp(const char* base, const Str& ext, Allocator allocator = allocator_top())
	{
		return file_tmp(str_lit(base), ext, allocator);
	}

	// returns a new file path inside the temproray folder using the given base name and extension
	// file_tmp("base", "png"): /tmp/base_3248239476.png
	inline static Str
	file_tmp(const Str& base, const char* ext, Allocator allocator = allocator_top())
	{
		return file_tmp(base, str_lit(ext), allocator);
	}

	// returns a new file path inside the temproray folder using the given base name and extension
	// file_tmp("base", "png"): /tmp/base_3248239476.png
	inline static Str
	file_tmp(const char* base, const char* ext, Allocator allocator = allocator_top())
	{
		return file_tmp(str_lit(base), str_lit(ext), allocator);
	}

	// creates a new folder and returns whether it succeeded
	MN_EXPORT bool
	folder_make(const char* path);

	// creates a new folder and returns whether it succeeded
	inline static bool
	folder_make(const Str& path)
	{
		return folder_make(path.ptr);
	}

	// creates a new folder and all its parents if don't exist and returns whether it succeeded
	inline static bool
	folder_make_recursive(const Str& path)
	{
		auto normal_path = path_normalize(str_clone(path, memory::tmp()));
		auto folders = str_split(normal_path, "/", true, memory::tmp());

		auto folder_to_make = str_tmp(str_prefix(path, "/")? "/" : nullptr);
		for (const auto& folder : folders)
		{
			folder_to_make = path_join(folder_to_make, folder);
			if (folder_make(folder_to_make) == false)
				return false;
		}

		return true;
	}

	// creates a new folder and all its parents if don't exist and returns whether it succeeded
	inline static bool
	folder_make_recursive(const char* path)
	{
		return folder_make_recursive(mn::str_lit(path));
	}

	// removes a new folder and returns whether it succeeded
	MN_EXPORT bool
	folder_remove(const char* path);

	// removes a new folder and returns whether it succeeded
	inline static bool
	folder_remove(const Str& path)
	{
		return folder_remove(path.ptr);
	}

	// copies a folder and the contained files/folders from src to dst, and returns whether it succeeded
	MN_EXPORT bool
	folder_copy(const char* src, const char* dst);

	// copies a folder and the contained files/folders from src to dst, and returns whether it succeeded
	inline static bool
	folder_copy(const Str& src, const char* dst)
	{
		return folder_copy(src.ptr, dst);
	}

	// copies a folder and the contained files/folders from src to dst, and returns whether it succeeded
	inline static bool
	folder_copy(const char* src, const Str& dst)
	{
		return folder_copy(src, dst.ptr);
	}

	// copies a folder and the contained files/folders from src to dst, and returns whether it succeeded
	inline static bool
	folder_copy(const Str& src, const Str& dst)
	{
		return folder_copy(src.ptr, dst.ptr);
	}

	// moves a folder and the contained files/folders from src to dst, and returns whether it succeeded
	inline static bool
	folder_move(const char* src, const char* dst)
	{
		if (folder_copy(src, dst))
			return folder_remove(src);
		return false;
	}

	// moves a folder and the contained files/folders from src to dst, and returns whether it succeeded
	inline static bool
	folder_move(const Str& src, const char* dst)
	{
		return folder_move(src.ptr, dst);
	}

	// moves a folder and the contained files/folders from src to dst, and returns whether it succeeded
	inline static bool
	folder_move(const char* src, const Str& dst)
	{
		return folder_move(src, dst.ptr);
	}

	// moves a folder and the contained files/folders from src to dst, and returns whether it succeeded
	inline static bool
	folder_move(const Str& src, const Str& dst)
	{
		return folder_move(src.ptr, dst.ptr);
	}

	// returns the path of the temporary folder
	MN_EXPORT Str
	folder_tmp(Allocator allocator = allocator_top());

	// returns the system configuration folder
	MN_EXPORT Str
	folder_config(Allocator allocator = allocator_top());
}
