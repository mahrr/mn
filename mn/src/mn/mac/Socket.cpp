#include "mn/Socket.h"
#include "mn/Fabric.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>

namespace mn
{
	inline static int
	_socket_family_to_os(SOCKET_FAMILY f)
	{
		switch (f)
		{
		case SOCKET_FAMILY_IPV4:
			return AF_INET;
		case SOCKET_FAMILY_IPV6:
			return AF_INET6;
		case SOCKET_FAMILY_UNSPEC:
			return AF_UNSPEC;
		default:
			mn_unreachable();
			return 0;
		}
	}

	inline static void
	_socket_type_to_os(SOCKET_TYPE t, int& type, int& protocol)
	{
		switch (t)
		{
		case SOCKET_TYPE_TCP:
			type = SOCK_STREAM;
			if(auto p = ::getprotobyname("tcp"))
				protocol = p->p_proto;
			break;
		case SOCKET_TYPE_UDP:
			type = SOCK_DGRAM;
			if(auto p = ::getprotobyname("udp"))
				protocol = p->p_proto;
			break;
		default:
			mn_unreachable();
			break;
		}
	}

	inline static MN_SOCKET_ERROR
	_socket_error_from_os(int error)
	{
		switch(error)
		{
		case ECONNREFUSED:
			return MN_SOCKET_ERROR_CONNECTION_CLOSED;
		case EFAULT:
		case EINVAL:
			return MN_SOCKET_ERROR_INTERNAL_ERROR;
		case ENOMEM:
			return MN_SOCKET_ERROR_OUT_OF_MEMORY;
		default:
			return MN_SOCKET_ERROR_GENERIC_ERROR;
		}
	}


	// API
	void
	ISocket::dispose()
	{
		socket_close(this);
	}

	size_t
	ISocket::read(Block data)
	{
		auto [read_bytes, _] = socket_read(this, data, INFINITE_TIMEOUT);
		return read_bytes;
	}

	size_t
	ISocket::write(Block data)
	{
		return socket_write(this, data);
	}

	int64_t
	ISocket::size()
	{
		return 0;
	}

	Socket
	socket_open(SOCKET_FAMILY socket_family, SOCKET_TYPE socket_type)
	{
		int af = 0;
		int type = 0;
		int protocol = 0;

		af = _socket_family_to_os(socket_family);
		_socket_type_to_os(socket_type, type, protocol);

		auto handle = ::socket(af, type, protocol);
		if (handle == -1)
			return nullptr;

		auto self = alloc_construct<ISocket>();
		self->handle = handle;
		self->family = socket_family;
		self->type = socket_type;
		return self;
	}

	void
	socket_close(Socket self)
	{
		::close(self->handle);
		free_destruct(self);
	}

	bool
	socket_connect(Socket self, const Str& address, const Str& port)
	{
		addrinfo hints{}, *info;

		hints.ai_family = _socket_family_to_os(self->family);
		_socket_type_to_os(self->type, hints.ai_socktype, hints.ai_protocol);

		worker_block_ahead();
		mn_defer{worker_block_clear();};
		int res = ::getaddrinfo(address.ptr, port.ptr, &hints, &info);
		if (res != 0)
			return false;
		mn_defer{::freeaddrinfo(info);};

		for (auto it = info; it; it = it->ai_next)
		{
			res = ::connect(self->handle, it->ai_addr, int(it->ai_addrlen));
			if (res != -1)
				return true;
		}

		return false;
	}

	bool
	socket_bind(Socket self, const Str& port)
	{
		addrinfo hints{}, *info;

		hints.ai_family = _socket_family_to_os(self->family);
		_socket_type_to_os(self->type, hints.ai_socktype, hints.ai_protocol);
		hints.ai_flags = AI_PASSIVE;

		int res = ::getaddrinfo(nullptr, port.ptr, &hints, &info);
		if (res != 0)
			return false;

		res = ::bind(self->handle, info->ai_addr, int(info->ai_addrlen));
		if (res == -1)
			return false;

		return true;
	}

	bool
	socket_listen(Socket self, int max_connections)
	{
		if (max_connections == 0)
			max_connections = SOMAXCONN;

		int res = ::listen(self->handle, max_connections);
		if (res == -1)
			return false;
		return true;
	}

	Socket
	socket_accept(Socket self, Timeout timeout)
	{
		pollfd pfd_read{};
		pfd_read.fd = self->handle;
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
		auto handle = ::accept(self->handle, nullptr, nullptr);
		if(handle == -1)
			return nullptr;

		auto other = alloc_construct<ISocket>();
		other->handle = handle;
		other->family = self->family;
		other->type = self->type;
		return other;
	}

	void
	socket_disconnect(Socket self)
	{
		::shutdown(self->handle, SHUT_WR);
	}

	Result<size_t, MN_SOCKET_ERROR>
	socket_read(Socket self, Block data, Timeout timeout)
	{
		pollfd pfd_read{};
		pfd_read.fd = self->handle;
		pfd_read.events = POLLIN;

		int milliseconds = 0;
		if (timeout == INFINITE_TIMEOUT)
			milliseconds = -1;
		else if (timeout == NO_TIMEOUT)
			milliseconds = 0;
		else
			milliseconds = int(timeout.milliseconds);

		ssize_t res = 0;
		worker_block_ahead();
		mn_defer{worker_block_clear();};
		int ready = ::poll(&pfd_read, 1, milliseconds);
		if(ready > 0)
		{
			res = ::recv(self->handle, data.ptr, data.size, 0);
			if (res == -1)
				return _socket_error_from_os(errno);
			else
				return res;
		}
		else if (ready == -1)
		{
			return _socket_error_from_os(errno);
		}
		else
		{
			return MN_SOCKET_ERROR_TIMEOUT;
		}
	}

	size_t
	socket_write(Socket self, Block data)
	{
		worker_block_ahead();
		auto res = ::send(self->handle, data.ptr, data.size, 0);
		worker_block_clear();
		if(res == -1)
			return 0;
		return res;
	}

	int64_t
	socket_fd(Socket self)
	{
		return self->handle;
	}
}
