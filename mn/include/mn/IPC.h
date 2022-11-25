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

	// sputnik is an inter-process communicateion protocol
	typedef struct ISputnik* Sputnik;

	struct ISputnik final : IStream
	{
		union
		{
			void* winos_named_pipe;
			int linux_domain_socket;
		};
		Str name;
		uint64_t read_msg_size;

		MN_EXPORT void
		dispose() override;

		MN_EXPORT size_t
		read(Block data) override;

		MN_EXPORT size_t
		write(Block data) override;

		MN_EXPORT int64_t
		size() override;

		virtual int64_t
		cursor_operation(STREAM_CURSOR_OP, int64_t) override
		{
			mn_unreachable_msg("sputnik doesn't support cursor operations");
			return STREAM_CURSOR_ERROR;
		}
	};

	// creates a new sputnik instance with the given name, if it fails it will return nullptr
	MN_EXPORT Sputnik
	sputnik_new(const Str& name);

	// creates a new sputnik instance with the given name, if it fails it will return nullptr
	inline static Sputnik
	sputnik_new(const char* name)
	{
		return sputnik_new(str_lit(name));
	}

	// connects to a given sputnik instance with the given name, if it fails it will return nullptr
	MN_EXPORT Sputnik
	sputnik_connect(const Str& name);

	// connects to a given sputnik instance with the given name, if it fails it will return nullptr
	inline static Sputnik
	sputnik_connect(const char* name)
	{
		return sputnik_connect(str_lit(name));
	}

	// frees the given sputnik instance
	MN_EXPORT void
	sputnik_free(Sputnik self);

	// starts listening for connection on the given sputnik instance
	MN_EXPORT bool
	sputnik_listen(Sputnik self);

	// tries to accept connection from the given sputnik instance within the given timeout window
	// if it fails it will return nullptr
	MN_EXPORT Sputnik
	sputnik_accept(Sputnik self, Timeout timeout);

	// tries to read from the given sputnik instance within the given timeout window
	// returns the number of read bytes
	MN_EXPORT size_t
	sputnik_read(Sputnik self, Block data, Timeout timeout);

	// writes the given block of bytes into the given sputnik instance and returns the number of written bytes
	MN_EXPORT size_t
	sputnik_write(Sputnik self, Block data);

	// disconnects the given sputnik instance
	MN_EXPORT bool
	sputnik_disconnect(Sputnik self);

	// sputnik message protocol
	// a message is an encapsulated binary blob of data which is transimetted over the sputnik stream

	// writes a message unit to sputnik which is {len: 8 bytes, the message}
	MN_EXPORT bool
	sputnik_msg_write(Sputnik self, Block data);

	struct Msg_Read_Return
	{
		size_t consumed;
		uint64_t remaining;
	};

	// reads a message unit from sputnik
	MN_EXPORT Msg_Read_Return
	sputnik_msg_read(Sputnik self, Block data, Timeout timeout);

	// allocates and reads a single message
	MN_EXPORT Str
	sputnik_msg_read_alloc(Sputnik self, Timeout timeout, Allocator allocator = allocator_top());
}