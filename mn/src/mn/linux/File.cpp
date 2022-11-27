#include "mn/File.h"
#include "mn/OS.h"
#include "mn/Fabric.h"
#include "mn/Assert.h"

#define _LARGEFILE64_SOURCE 1
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <linux/limits.h>

namespace mn
{
	File
	_file_stdout()
	{
		static IFile _stdout{};
		_stdout.linux_handle = STDOUT_FILENO;
		return &_stdout;
	}

	File
	_file_stderr()
	{
		static IFile _stderr{};
		_stderr.linux_handle = STDERR_FILENO;
		return &_stderr;
	}

	File
	_file_stdin()
	{
		static IFile _stdin{};
		_stdin.linux_handle = STDIN_FILENO;
		return &_stdin;
	}

	inline static bool
	_is_std_file(int h)
	{
		return (
			h == _file_stdout()->linux_handle ||
			h == _file_stderr()->linux_handle ||
			h == _file_stdin()->linux_handle
		);
	}

	//API
	void
	IFile::dispose()
	{
		if (linux_handle != -1 &&
			_is_std_file(linux_handle) == false)
		{
			::close(linux_handle);
		}
		free(this);
	}

	Result<size_t, IO_ERROR>
	IFile::read(Block data)
	{
		worker_block_ahead();
		auto res = ::read(linux_handle, data.ptr, data.size);
		worker_block_clear();
		if (res == -1)
			return IO_ERROR_UNKNOWN;
		return res;
	}

	Result<size_t, IO_ERROR>
	IFile::write(Block data)
	{
		worker_block_ahead();
		auto res = ::write(linux_handle, data.ptr, data.size);
		worker_block_clear();
		if (res == -1)
			return IO_ERROR_UNKNOWN;
		return res;
	}

	Result<size_t, IO_ERROR>
	IFile::size()
	{
		struct stat file_stats;
		if(::fstat(linux_handle, &file_stats) == 0)
		{
			return file_stats.st_size;
		}
		return IO_ERROR_UNKNOWN;
	}

	Block
	to_os_encoding(const Str& utf8, Allocator allocator)
	{
		return block_clone(block_from(utf8), allocator);
	}

	Block
	to_os_encoding(const char* utf8, Allocator allocator)
	{
		return to_os_encoding(str_lit(utf8), allocator);
	}

	Str
	from_os_encoding(Block os_str, Allocator allocator)
	{
		return str_from_c((char*)os_str.ptr, allocator);
	}

	File
	file_stdout()
	{
		static File _f = _file_stdout();
		return _f;
	}

	File
	file_stderr()
	{
		static File _f = _file_stderr();
		return _f;
	}

	File
	file_stdin()
	{
		static File _f = _file_stdin();
		return _f;
	}

	File
	file_open(const char* filename, IO_MODE io_mode, OPEN_MODE open_mode, SHARE_MODE share_mode)
	{
		int flags = 0;

		//translate the io mode
		switch(io_mode)
		{
			case IO_MODE_READ:
				flags |= O_RDONLY;
				break;

			case IO_MODE_WRITE:
				flags |= O_WRONLY;
				break;

			case IO_MODE_READ_WRITE:
			default:
				flags |= O_RDWR;
				break;
		}

		//translate the open mode
		switch(open_mode)
		{
			case OPEN_MODE_CREATE_ONLY:
				flags |= O_CREAT;
				flags |= O_EXCL;
				break;

			case OPEN_MODE_CREATE_APPEND:
				flags |= O_CREAT;
				flags |= O_APPEND;
				break;

			case OPEN_MODE_OPEN_ONLY:
				//do nothing
				break;

			case OPEN_MODE_OPEN_OVERWRITE:
				flags |= O_TRUNC;
				break;

			case OPEN_MODE_OPEN_APPEND:
				flags |= O_APPEND;
				break;

			case OPEN_MODE_CREATE_OVERWRITE:
			default:
				flags |= O_CREAT;
				flags |= O_TRUNC;
				break;
		}

		// Linux doesn't support the granularity of file sharing like windows so we only support
		// NONE which is available only in O_CREAT mode
		switch(share_mode)
		{
			case SHARE_MODE_NONE:
				if(flags & O_CREAT)
					flags |= O_EXCL;
				break;

			default:
				break;
		}

		int linux_handle = ::open(filename, flags, S_IRWXU);
		if(linux_handle == -1)
			return nullptr;

		File self = alloc_construct<IFile>();
		self->linux_handle = linux_handle;
		return self;
	}

	void
	file_close(File self)
	{
		self->dispose();
	}

	bool
	file_valid(File self)
	{
		return self->linux_handle != -1;
	}

	Result<size_t, IO_ERROR>
	file_write(File self, Block data)
	{
		return self->write(data);
	}

	Result<size_t, IO_ERROR>
	file_read(File self, Block data)
	{
		return self->read(data);
	}

	Result<size_t, IO_ERROR>
	file_size(File self)
	{
		struct stat file_stats;
		if(::fstat(self->linux_handle, &file_stats) == 0)
		{
			return file_stats.st_size;
		}
		return IO_ERROR_UNKNOWN;
	}

	Result<size_t, IO_ERROR>
	file_cursor_pos(File self)
	{
		off64_t offset = 0;
		return ::lseek64(self->linux_handle, offset, SEEK_CUR);
	}

	bool
	file_cursor_move(File self, int64_t move_offset)
	{
		off64_t offset = move_offset;
		return ::lseek64(self->linux_handle, offset, SEEK_CUR) != -1;
	}

	bool
	file_cursor_set(File self, int64_t absolute)
	{
		off64_t offset = absolute;
		return ::lseek64(self->linux_handle, offset, SEEK_SET) != -1;
	}

	bool
	file_cursor_move_to_start(File self)
	{
		off64_t offset = 0;
		return ::lseek64(self->linux_handle, offset, SEEK_SET) != -1;
	}

	bool
	file_cursor_move_to_end(File self)
	{
		off64_t offset = 0;
		return ::lseek64(self->linux_handle, offset, SEEK_END) != -1;
	}

	bool
	file_write_try_lock(File self, int64_t offset, int64_t size)
	{
		mn_assert(offset >= 0 && size >= 0);
		flock fl{};
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(self->linux_handle, F_SETLK, &fl) != -1;
	}

	void
	file_write_lock(File handle, int64_t offset, int64_t size)
	{
		worker_block_on([&]{
			return file_write_try_lock(handle, offset, size);
		});
	}

	bool
	file_write_unlock(File self, int64_t offset, int64_t size)
	{
		mn_assert(offset >= 0 && size >= 0);
		flock fl{};
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(self->linux_handle, F_SETLK, &fl) != -1;
	}

	bool
	file_read_try_lock(File self, int64_t offset, int64_t size)
	{
		mn_assert(offset >= 0 && size >= 0);
		flock fl{};
		fl.l_type = F_RDLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(self->linux_handle, F_SETLK, &fl) != -1;
	}

	void
	file_read_lock(File handle, int64_t offset, int64_t size)
	{
		worker_block_on([&]{
			return file_read_try_lock(handle, offset, size);
		});
	}

	bool
	file_read_unlock(File self, int64_t offset, int64_t size)
	{
		mn_assert(offset >= 0 && size >= 0);
		flock fl{};
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(self->linux_handle, F_SETLK, &fl) != -1;
	}

	struct IMapped_File
	{
		Mapped_File file_view;
		// if set this means that the mapped file owns it
		File mn_file_handle;
	};

	Mapped_File*
	file_mmap(File file, int64_t offset, int64_t size, IO_MODE io_mode)
	{
		int prot = PROT_READ;
		int flags = MAP_PRIVATE;
		switch (io_mode)
		{
		case IO_MODE_READ:
			prot = PROT_READ;
			flags = MAP_PRIVATE;
			break;
		case IO_MODE_WRITE:
			prot = PROT_WRITE;
			flags = MAP_SHARED;
			break;
		case IO_MODE_READ_WRITE:
			prot = PROT_READ | PROT_WRITE;
			flags = MAP_SHARED;
			break;
		default:
			mn_unreachable();
			break;
		}

		auto [filesize, err] = file_size(file);
		if (err)
			return nullptr;

		if (size == 0)
		{
			size = filesize - offset;
		}
		else if (size_t(size) > filesize)
		{
			auto res = ::ftruncate64(file->linux_handle, offset + size);
			if (res != 0)
				return nullptr;
		}

		auto ptr = ::mmap(
			NULL,
			size,
			prot,
			flags,
			file->linux_handle,
			offset
		);

		if (ptr == nullptr)
			return nullptr;

		auto self = alloc_zerod<IMapped_File>();
		self->file_view.data.ptr = ptr;
		self->file_view.data.size = size;

		return &self->file_view;
	}

	Mapped_File*
	file_mmap(const Str& filename, int64_t offset, int64_t size, IO_MODE io_mode, OPEN_MODE open_mode, SHARE_MODE share_mode)
	{
		auto file = file_open(filename, io_mode, open_mode, share_mode);
		if (file == nullptr)
			return nullptr;
		mn_defer{if (file) file_close(file);};

		auto res = file_mmap(file, offset, size, io_mode);
		if (res == nullptr)
			return nullptr;

		auto self = (IMapped_File*)res;
		self->mn_file_handle = file;
		file = nullptr;
		return res;
	}

	bool
	file_unmap(Mapped_File* ptr)
	{
		auto self = (IMapped_File*)ptr;
		auto res = ::munmap(self->file_view.data.ptr, self->file_view.data.size);
		if (self->mn_file_handle)
			file_close(self->mn_file_handle);
		free(self);
		return res == 0;
	}
}
