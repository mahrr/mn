#include "mn/File.h"
#include "mn/Defer.h"
#include "mn/Memory.h"
#include "mn/Thread.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mbstring.h>
#include <tchar.h>

namespace mn
{
	Str
	_from_os_encoding(Block os_str, Allocator allocator)
	{
		int size_needed = WideCharToMultiByte(CP_UTF8, NULL, (LPWSTR)os_str.ptr,
			int(os_str.size / sizeof(WCHAR)), NULL, 0, NULL, NULL);
		if (size_needed == 0)
			return str_with_allocator(allocator);

		Str buffer = str_with_allocator(allocator);
		buf_resize(buffer, size_needed);

		size_needed = WideCharToMultiByte(CP_UTF8, NULL, (LPWSTR)os_str.ptr,
			int(os_str.size / sizeof(WCHAR)), buffer.ptr, int(buffer.count), NULL, NULL);
		--buffer.count;
		return buffer;
	}

	inline static IFile
	_file_stdout()
	{
		constexpr const uint32_t MY_ENABLE_VIRTUAL_TERMINAL_PROCESSING = 4;

		IFile file;
		file.winos_handle = GetStdHandle(STD_OUTPUT_HANDLE);

		DWORD mode;
		GetConsoleMode(file.winos_handle, &mode);
		mode |= MY_ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(file.winos_handle, mode);
		return file;
	}

	struct Mutex_Stdout_Wrapper
	{
		Mutex mtx;

		Mutex_Stdout_Wrapper()
		{
			allocator_push(memory::clib());
				mtx = mutex_new("Stdout Mutex");
			allocator_pop();
		}

		~Mutex_Stdout_Wrapper()
		{
			allocator_push(memory::clib());
				mutex_free(mtx);
			allocator_pop();
		}
	};

	inline static Mutex
	_mutex_stdout()
	{
		static Mutex_Stdout_Wrapper wrapper;
		return wrapper.mtx;
	}

	inline static IFile
	_file_stderr()
	{
		IFile file;
		file.winos_handle = GetStdHandle(STD_ERROR_HANDLE);
		return file;
	}

	struct Mutex_Stderr_Wrapper
	{
		Mutex mtx;

		Mutex_Stderr_Wrapper()
		{
			allocator_push(memory::clib());
				mtx = mutex_new("Stderr Mutex");
			allocator_pop();
		}

		~Mutex_Stderr_Wrapper()
		{
			allocator_push(memory::clib());
				mutex_free(mtx);
			allocator_pop();
		}
	};

	inline static Mutex
	_mutex_stderr()
	{
		static Mutex_Stderr_Wrapper wrapper;
		return wrapper.mtx;
	}

	struct Mutex_Stdin_Wrapper
	{
		Mutex mtx;

		Mutex_Stdin_Wrapper()
		{
			mtx = mutex_new("Stdin Mutex");
		}

		~Mutex_Stdin_Wrapper()
		{
			mutex_free(mtx);
		}
	};

	inline static Mutex
	_mutex_stdin()
	{
		static Mutex_Stdin_Wrapper wrapper;
		return wrapper.mtx;
	}

	inline static IFile
	_file_stdin()
	{
		IFile file;
		file.winos_handle = GetStdHandle(STD_INPUT_HANDLE);
		return file;
	}


	//API
	IFile::~IFile()
	{
		if(winos_handle != INVALID_HANDLE_VALUE)
			CloseHandle(winos_handle);
	}

	size_t
	IFile::read(Block data)
	{
		DWORD bytes_read = 0;
		Mutex mtx = nullptr;
		if(winos_handle == file_stdin()->winos_handle)
			mtx = _mutex_stdin();

		if(mtx) mutex_lock(mtx);
			ReadFile(winos_handle, data.ptr, DWORD(data.size), &bytes_read, NULL);
		if(mtx) mutex_unlock(mtx);
		return bytes_read;
	}

	size_t
	IFile::write(Block data)
	{
		DWORD bytes_written = 0;

		Mutex mtx = nullptr;
		if(winos_handle == file_stdout()->winos_handle)
			mtx = _mutex_stdout();
		else if(winos_handle == file_stderr()->winos_handle)
			mtx = _mutex_stderr();

		if (mtx) mutex_lock(mtx);
			WriteFile(winos_handle, data.ptr, DWORD(data.size), &bytes_written, NULL);
		if (mtx) mutex_unlock(mtx);
		return bytes_written;
	}

	//helpers
	Block
	to_os_encoding(const Str& utf8)
	{
		int size_needed = MultiByteToWideChar(CP_UTF8,
			MB_PRECOMPOSED, utf8.ptr, int(utf8.count), NULL, 0);

		//+1 for the null termination
		size_t required_size = (size_needed + 1) * sizeof(WCHAR);
		Block buffer = alloc(required_size, alignof(WCHAR));

		size_needed = MultiByteToWideChar(CP_UTF8,
			MB_PRECOMPOSED, utf8.ptr, int(utf8.cap), (LPWSTR)buffer.ptr, int(buffer.size));
		return buffer;
	}

	Block
	to_os_encoding(const char* utf8)
	{
		return to_os_encoding(str_lit(utf8));
	}

	Str
	from_os_encoding(Block os_str, Allocator allocator)
	{
		return _from_os_encoding(os_str, allocator);
	}


	//std files
	File
	file_stdout()
	{
		static IFile _stdout = _file_stdout();
		return &_stdout;
	}

	File
	file_stderr()
	{
		static IFile _stderr = _file_stderr();
		return &_stderr;
	}

	File
	file_stdin()
	{
		static IFile _stdin = _file_stdin();
		return &_stdin;
	}


	//files
	File
	file_open(const char* filename, IO_MODE io_mode, OPEN_MODE open_mode)
	{
		//translate the io mode
		DWORD desired_access;
		switch(io_mode)
		{
			case IO_MODE::READ:
				desired_access = GENERIC_READ;
				break;

			case IO_MODE::WRITE:
				desired_access = GENERIC_WRITE;
				break;

			case IO_MODE::READ_WRITE:
			default:
				desired_access = GENERIC_READ | GENERIC_WRITE;
				break;
		}

		//translate the open mode
		DWORD creation_disposition;
		switch(open_mode)
		{
			case OPEN_MODE::CREATE_ONLY:
				creation_disposition = CREATE_NEW;
				break;

			case OPEN_MODE::OPEN_OVERWRITE:
				creation_disposition = TRUNCATE_EXISTING;
				break;

			case OPEN_MODE::OPEN_ONLY:
			case OPEN_MODE::OPEN_APPEND:
				creation_disposition = OPEN_EXISTING;
				break;

			case OPEN_MODE::CREATE_APPEND:
				creation_disposition = OPEN_ALWAYS;
				break;

			case OPEN_MODE::CREATE_OVERWRITE:
			default:
				creation_disposition = CREATE_ALWAYS;
				break;
		}

		Block os_str = to_os_encoding(filename);
		mn_defer(free(os_str));
		LPWSTR win_filename = (LPWSTR)os_str.ptr;
		HANDLE windows_handle = CreateFile (win_filename, desired_access, 0, NULL,
											creation_disposition,
											FILE_ATTRIBUTE_NORMAL,
											NULL);

		if(windows_handle == INVALID_HANDLE_VALUE)
			return nullptr;

		if(open_mode == OPEN_MODE::CREATE_APPEND ||
		   open_mode == OPEN_MODE::OPEN_APPEND)
		{
			SetFilePointer (windows_handle,	//file handle
							NULL,					//distance to move low part
				 			NULL,					//ditance to mvoe high part
				 			FILE_END); 				//movement point of reference
		}

		File self = alloc_construct<IFile>();
		self->winos_handle = windows_handle;
		return self;
	}

	void
	file_close(File self)
	{
		free_destruct(self);
	}

	bool
	file_valid(File self)
	{
		return self->winos_handle != INVALID_HANDLE_VALUE;
	}

	size_t
	file_write(File self, Block data)
	{
		return self->write(data);
	}

	size_t
	file_read(File self, Block data)
	{
		return self->read(data);
	}

	int64_t
	file_size(File self)
	{
		LARGE_INTEGER size;
		if(GetFileSizeEx(self->winos_handle, &size))
		{
			return *(int64_t*)(&size);
		}
		return -1;
	}

	int64_t
	file_cursor_pos(File self)
	{
		LARGE_INTEGER position, offset;
		offset.QuadPart = 0;
		if(SetFilePointerEx(self->winos_handle, offset, &position, FILE_CURRENT))
		{
			return *(int64_t*)(&position);
		}
		return -1;
	}

	bool
	file_cursor_move(File self, int64_t offset)
	{
		LARGE_INTEGER position, win_offset;
		win_offset.QuadPart = offset;
		return SetFilePointerEx(self->winos_handle, win_offset, &position, FILE_CURRENT);
	}

	bool
	file_cursor_set(File self, int64_t absolute)
	{
		LARGE_INTEGER position, win_offset;
		win_offset.QuadPart = absolute;
		return SetFilePointerEx(self->winos_handle, win_offset, &position, FILE_BEGIN);
	}

	bool
	file_cursor_move_to_start(File self)
	{
		LARGE_INTEGER position, offset;
		offset.QuadPart = 0;
		return SetFilePointerEx(self->winos_handle, offset, &position, FILE_BEGIN);
	}

	bool
	file_cursor_move_to_end(File self)
	{
		LARGE_INTEGER position, offset;
		offset.QuadPart = 0;
		return SetFilePointerEx(self->winos_handle, offset, &position, FILE_END);
	}
}