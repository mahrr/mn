#include "mn/IPC.h"
#include "mn/Str.h"
#include "mn/Memory.h"
#include "mn/Fabric.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>

namespace mn::ipc
{
	bool
	_mutex_try_lock(Mutex self, int64_t offset, int64_t size)
	{
		mn_assert(offset >= 0 && size >= 0);
		struct flock fl{};
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(intptr_t(self), F_SETLK, &fl) != -1;
	}

	bool
	_mutex_unlock(Mutex self, int64_t offset, int64_t size)
	{
		mn_assert(offset >= 0 && size >= 0);
		struct flock fl{};
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(intptr_t(self), F_SETLK, &fl) != -1;
	}

	inline static IO_ERROR
	_ipc_error_from_os(int error)
	{
		switch(error)
		{
		case ECONNREFUSED:
			return IO_ERROR_CLOSED;
		case EFAULT:
		case EINVAL:
			return IO_ERROR_INTERNAL_ERROR;
		case ENOMEM:
			return IO_ERROR_OUT_OF_MEMORY;
		default:
			return IO_ERROR_UNKNOWN;
		}
	}

	// API
	Mutex
	mutex_new(const Str& name)
	{
		int flags = O_WRONLY | O_CREAT | O_APPEND;

		int macos_handle = ::open(name.ptr, flags, S_IRWXU);
		if(macos_handle == -1)
			return nullptr;

		return Mutex((intptr_t)macos_handle);
	}

	void
	mutex_free(Mutex mtx)
	{
		::close(intptr_t(mtx));
	}

	void
	mutex_lock(Mutex mtx)
	{
		worker_block_ahead();

		worker_block_on([&]{
			return _mutex_try_lock(mtx, 0, 0);
		});

		worker_block_clear();
	}

	bool
	mutex_try_lock(Mutex mtx)
	{
		return _mutex_try_lock(mtx, 0, 0);
	}

	void
	mutex_unlock(Mutex mtx)
	{
		_mutex_unlock(mtx, 0, 0);
	}

	void
	ILocal_Socket::dispose()
	{
		local_socket_free(this);
	}

	Result<size_t, IO_ERROR>
	ILocal_Socket::read(Block data)
	{
		return local_socket_read(this, data, INFINITE_TIMEOUT);
	}

	Result<size_t, IO_ERROR>
	ILocal_Socket::write(Block data)
	{
		return local_socket_write(this, data, INFINITE_TIMEOUT);
	}

	Local_Socket
	local_socket_new(const Str& name)
	{
		sockaddr_un addr{};
		addr.sun_family = AF_LOCAL;
		mn_assert_msg(name.count < sizeof(addr.sun_path), "name is too long");
		size_t name_length = name.count;
		if(name.count >= sizeof(addr.sun_path))
			name_length = sizeof(addr.sun_path);
		::memcpy(addr.sun_path, name.ptr, name_length);

		int handle = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if(handle < 0)
			return nullptr;

		::unlink(addr.sun_path);
		auto res = ::bind(handle, (sockaddr*)&addr, SUN_LEN(&addr));
		if(res < 0)
		{
			::close(handle);
			return nullptr;
		}
		auto self = alloc_construct<ILocal_Socket>();
		self->linux_domain_socket = handle;
		self->name = str_from_substr(name.ptr, name.ptr + name_length);
		return self;
	}

	Local_Socket
	local_socket_connect(const Str& name)
	{
		sockaddr_un addr{};
		addr.sun_family = AF_LOCAL;
		mn_assert_msg(name.count < sizeof(addr.sun_path), "name is too long");
		size_t name_length = name.count;
		if(name.count >= sizeof(addr.sun_path))
			name_length = sizeof(addr.sun_path);
		::memcpy(addr.sun_path, name.ptr, name_length);

		int handle = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if(handle < 0)
			return nullptr;

		worker_block_ahead();
		if(::connect(handle, (sockaddr*)&addr, sizeof(sockaddr_un)) < 0)
		{
			::close(handle);
			return nullptr;
		}
		worker_block_clear();

		auto self = alloc_construct<ILocal_Socket>();
		self->linux_domain_socket = handle;
		self->name = str_from_substr(name.ptr, name.ptr + name_length);
		return self;
	}

	void
	local_socket_free(Local_Socket self)
	{
		::close(self->linux_domain_socket);
		str_free(self->name);
		free_destruct(self);
	}

	bool
	local_socket_listen(Local_Socket self)
	{
		worker_block_ahead();
		int res = ::listen(self->linux_domain_socket, SOMAXCONN);
		worker_block_clear();
		if (res == -1)
			return false;
		return true;
	}

	Local_Socket
	local_socket_accept(Local_Socket self, Timeout timeout)
	{
		pollfd pfd_read{};
		pfd_read.fd = self->linux_domain_socket;
		pfd_read.events = POLLIN;

		int milliseconds = 0;
		if(timeout == INFINITE_TIMEOUT)
			milliseconds = -1;
		else if(timeout == NO_TIMEOUT)
			milliseconds = 0;
		else
			milliseconds = int(timeout.milliseconds);

		{
			worker_block_ahead();
			mn_defer{worker_block_clear();};

			int ready = poll(&pfd_read, 1, milliseconds);
			if(ready == 0)
				return nullptr;
		}
		auto handle = ::accept(self->linux_domain_socket, 0, 0);
		if(handle == -1)
			return nullptr;
		auto other = alloc_construct<ILocal_Socket>();
		other->linux_domain_socket = handle;
		other->name = clone(self->name);
		return other;
	}

	Result<size_t, IO_ERROR>
	local_socket_read(Local_Socket self, Block data, Timeout timeout)
	{
		pollfd pfd_read{};
		pfd_read.fd = self->linux_domain_socket;
		pfd_read.events = POLLIN;

		int milliseconds = 0;
		if(timeout == INFINITE_TIMEOUT)
			milliseconds = -1;
		else if(timeout == NO_TIMEOUT)
			milliseconds = 0;
		else
			milliseconds = int(timeout.milliseconds);

		ssize_t res = 0;
		worker_block_ahead();
		int ready = poll(&pfd_read, 1, milliseconds);
		if(ready > 0)
			res = ::read(self->linux_domain_socket, data.ptr, data.size);
		worker_block_clear();
		if(res == -1)
			return _ipc_error_from_os(errno);
		return res;
	}

	Result<size_t, IO_ERROR>
	local_socket_write(Local_Socket self, Block data, Timeout timeout)
	{
		pollfd pfd_write{};
		pfd_write.fd = self->linux_domain_socket;
		pfd_write.events = POLLOUT;

		int milliseconds = 0;
		if(timeout == INFINITE_TIMEOUT)
			milliseconds = -1;
		else if(timeout == NO_TIMEOUT)
			milliseconds = 0;
		else
			milliseconds = int(timeout.milliseconds);

		ssize_t res = 0;
		worker_block_ahead();
		int ready = poll(&pfd_write, 1, milliseconds);
		if(ready > 0)
			res = ::write(self->linux_domain_socket, data.ptr, data.size);
		worker_block_clear();
		if(res == -1)
			return _ipc_error_from_os(errno);
		return res;
	}

	bool
	local_socket_disconnect(Local_Socket self)
	{
		return ::unlink(self->name.ptr) == 0;
	}
}
