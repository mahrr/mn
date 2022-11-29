#pragma once

#include "mn/Exports.h"
#include "mn/Str.h"
#include "mn/Stream.h"
#include "mn/Assert.h"

namespace mn::ipc
{
	// an inter-process mutex which can be used to sync multiple processes
	typedef struct IIPC_Mutex *Mutex;

	// creates a new inter-process mutex with the given name
	MN_EXPORT Mutex
	mutex_new(const Str& name);

	// creates a new inter-process mutex with the given name
	inline static Mutex
	mutex_new(const char* name)
	{
		return mutex_new(str_lit(name));
	}

	// frees the given mutex
	MN_EXPORT void
	mutex_free(Mutex self);

	// destruct overload for mutex free
	inline static void
	destruct(Mutex self)
	{
		mutex_free(self);
	}

	// locks the given mutex
	MN_EXPORT void
	mutex_lock(Mutex self);

	// tries to lock the given mutex, and returns whether it has succeeded
	MN_EXPORT bool
	mutex_try_lock(Mutex self);

	// unlocks the given mutex
	MN_EXPORT void
	mutex_unlock(Mutex self);

	// OS communication primitives

	typedef struct ILocal_Socket* Local_Socket;

	struct ILocal_Socket final : IStream
	{
		union
		{
			void* winos_named_pipe;
			int linux_domain_socket;
		};
		Str name;

		MN_EXPORT void
		dispose() override;

		MN_EXPORT Result<size_t, IO_ERROR>
		read(Block data) override;

		MN_EXPORT Result<size_t, IO_ERROR>
		write(Block data) override;

		Result<size_t, IO_ERROR>
		size() override
		{
			return IO_ERROR_NOT_SUPPORTED;
		}

		Result<size_t, IO_ERROR>
		cursor_operation(STREAM_CURSOR_OP, int64_t) override
		{
			return IO_ERROR_NOT_SUPPORTED;
		}
	};

	// creates a new local socket instance with the given name, if it fails it will return nullptr
	MN_EXPORT Local_Socket
	local_socket_new(const Str& name);

	// creates a new local socket instance with the given name, if it fails it will return nullptr
	inline static Local_Socket
	local_socket_new(const char* name)
	{
		return local_socket_new(str_lit(name));
	}

	// connects to a given local socket instance with the given name, if it fails it will return nullptr
	MN_EXPORT Local_Socket
	local_socket_connect(const Str& name);

	// connects to a given local socket instance with the given name, if it fails it will return nullptr
	inline static Local_Socket
	local_socket_connect(const char* name)
	{
		return local_socket_connect(str_lit(name));
	}

	// frees the given local socket instance
	MN_EXPORT void
	local_socket_free(Local_Socket self);

	// starts listening for connection on the given local socket instance
	MN_EXPORT bool
	local_socket_listen(Local_Socket self);

	// tries to accept connection from the given local socket instance within the given timeout window
	// if it fails it will return nullptr
	MN_EXPORT Local_Socket
	local_socket_accept(Local_Socket self, Timeout timeout);

	// tries to read from the given local socket instance within the given timeout window
	// returns the number of read bytes
	MN_EXPORT Result<size_t, IO_ERROR>
	local_socket_read(Local_Socket self, Block data, Timeout timeout);

	// writes the given block of bytes into the given local socket instance and returns the number of written bytes
	MN_EXPORT Result<size_t, IO_ERROR>
	local_socket_write(Local_Socket self, Block data);

	// disconnects the given local socket instance
	MN_EXPORT bool
	local_socket_disconnect(Local_Socket self);
}