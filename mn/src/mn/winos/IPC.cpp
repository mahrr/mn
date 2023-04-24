#include "mn/IPC.h"
#include "mn/File.h"
#include "mn/Defer.h"
#include "mn/Fabric.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mbstring.h>
#include <tchar.h>

namespace mn::ipc
{
	inline static IO_ERROR
	_get_last_error()
	{
		auto err = GetLastError();
		if (err == 0)
			return IO_ERROR_NONE;

		switch (err)
		{
		case ERROR_ACCESS_DENIED:
		case ERROR_SHARING_VIOLATION:
			return IO_ERROR_PERMISSION_DENIED;
		case ERROR_NO_DATA:
		case ERROR_BROKEN_PIPE:
			return IO_ERROR_CLOSED;
		case ERROR_TIMEOUT:
		case WAIT_TIMEOUT:
			return IO_ERROR_TIMEOUT;
		default:
			return IO_ERROR_UNKNOWN;
		}
	}

	// API
	Mutex
	mutex_new(const Str& name)
	{
		auto os_str = to_os_encoding(name, allocator_top());
		mn_defer{free(os_str);};

		auto handle = CreateMutex(0, false, (LPCWSTR)os_str.ptr);
		if (handle == INVALID_HANDLE_VALUE)
			return nullptr;

		return (Mutex)handle;
	}

	void
	mutex_free(Mutex self)
	{
		[[maybe_unused]] auto res = CloseHandle((HANDLE)self);
		mn_assert(res == TRUE);
	}

	void
	mutex_lock(Mutex mtx)
	{
		auto self = (HANDLE)mtx;
		worker_block_ahead();
		WaitForSingleObject(self, INFINITE);
		worker_block_clear();
	}

	bool
	mutex_try_lock(Mutex mtx)
	{
		auto self = (HANDLE)mtx;
		auto res = WaitForSingleObject(self, 0);
		switch(res)
		{
		case WAIT_OBJECT_0:
		case WAIT_ABANDONED:
			return true;
		default:
			return false;
		}
	}

	void
	mutex_unlock(Mutex mtx)
	{
		auto self = (HANDLE)mtx;
		[[maybe_unused]] BOOL res = ReleaseMutex(self);
		mn_assert(res == TRUE);
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
		auto pipename = to_os_encoding(str_tmpf("\\\\.\\pipe\\{}", name));
		auto handle = CreateNamedPipe(
			(LPCWSTR)pipename.ptr,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
			PIPE_UNLIMITED_INSTANCES,
			4ULL * 1024ULL,
			4ULL * 1024ULL,
			NMPWAIT_USE_DEFAULT_WAIT,
			NULL
		);
		if (handle == INVALID_HANDLE_VALUE)
			return nullptr;
		auto self = alloc_construct<ILocal_Socket>();
		self->winos_named_pipe = handle;
		self->name = clone(name);
		return self;
	}

	Local_Socket
	local_socket_connect(const Str& name)
	{
		auto pipename = to_os_encoding(str_tmpf("\\\\.\\pipe\\{}", name));
		auto handle = CreateFile(
			(LPCWSTR)pipename.ptr,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			NULL
		);
		if (handle == INVALID_HANDLE_VALUE)
			return nullptr;

		auto self = alloc_construct<ILocal_Socket>();
		self->winos_named_pipe = handle;
		self->name = clone(name);
		return self;
	}

	void
	local_socket_free(Local_Socket self)
	{
		[[maybe_unused]] auto res = CloseHandle((HANDLE)self->winos_named_pipe);
		mn_assert(res == TRUE);
		str_free(self->name);
		free_destruct(self);
	}

	bool
	local_socket_listen(Local_Socket)
	{
		// this function doesn't map to anything on windows since in socket api it's used to change the state of a socket
		// to be able to accept connections, in named pipes on windows however this is unnecessary
		return true;
	}

	Local_Socket
	local_socket_accept(Local_Socket self, Timeout timeout)
	{
		// Wait for the Connection
		{
			OVERLAPPED overlapped{};
			overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			mn_defer{CloseHandle(overlapped.hEvent);};

			worker_block_ahead();
			mn_defer{worker_block_clear();};

			auto connected = ConnectNamedPipe((HANDLE)self->winos_named_pipe, &overlapped);
			auto last_error = GetLastError();
			if (connected == FALSE && last_error != ERROR_PIPE_CONNECTED)
			{
				if (last_error != ERROR_IO_PENDING)
					return nullptr;

				DWORD milliseconds = 0;
				if (timeout == INFINITE_TIMEOUT)
					milliseconds = INFINITE;
				else if (timeout == NO_TIMEOUT)
					milliseconds = 0;
				else
					milliseconds = DWORD(timeout.milliseconds);

				auto wakeup = WaitForSingleObject(overlapped.hEvent, milliseconds);
				if (wakeup != WAIT_OBJECT_0)
					return nullptr;
			}
		}

		// accept the connection
		auto pipename = to_os_encoding(str_tmpf("\\\\.\\pipe\\{}", self->name));
		auto handle = CreateNamedPipe(
			(LPCWSTR)pipename.ptr,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
			PIPE_UNLIMITED_INSTANCES,
			4ULL * 1024ULL,
			4ULL * 1024ULL,
			NMPWAIT_USE_DEFAULT_WAIT,
			NULL
		);
		if (handle == INVALID_HANDLE_VALUE)
			return nullptr;
		auto other = alloc_construct<ILocal_Socket>();
		other->winos_named_pipe = self->winos_named_pipe;
		other->name = clone(self->name);

		self->winos_named_pipe = handle;

		return other;
	}

	Result<size_t, IO_ERROR>
	local_socket_read(Local_Socket self, Block data, Timeout timeout)
	{
		DWORD bytes_read = 0;
		OVERLAPPED overlapped{};
		overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		worker_block_ahead();
		mn_defer { worker_block_clear(); };

		auto done = ReadFile(
			(HANDLE)self->winos_named_pipe,
			data.ptr,
			DWORD(data.size),
			&bytes_read,
			&overlapped
		);

		if (done)
			return bytes_read;
		else if (GetLastError() != ERROR_IO_PENDING)
			return _get_last_error();

		DWORD milliseconds = 0;
		if (timeout == INFINITE_TIMEOUT)
			milliseconds = INFINITE;
		else if (timeout == NO_TIMEOUT)
			milliseconds = 0;
		else
			milliseconds = DWORD(timeout.milliseconds);
		auto wakeup = WaitForSingleObject(overlapped.hEvent, milliseconds);

		if (wakeup == WAIT_TIMEOUT)
		{
			CancelIo(self->winos_named_pipe);
			return IO_ERROR_TIMEOUT;
		}
		CloseHandle(overlapped.hEvent);
		return overlapped.InternalHigh;
	}

	Result<size_t, IO_ERROR>
	local_socket_write(Local_Socket self, Block data, Timeout timeout)
	{
		DWORD bytes_written = 0;
		OVERLAPPED overlapped{};
		overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		worker_block_ahead();
		mn_defer { worker_block_clear(); };

		auto done = WriteFile(
			(HANDLE)self->winos_named_pipe,
			data.ptr,
			DWORD(data.size),
			&bytes_written,
			&overlapped
		);

		if (done)
			return bytes_written;
		else if (GetLastError() != ERROR_IO_PENDING)
			return _get_last_error();

		DWORD milliseconds = 0;
		if (timeout == INFINITE_TIMEOUT)
			milliseconds = INFINITE;
		else if (timeout == NO_TIMEOUT)
			milliseconds = 0;
		else
			milliseconds = DWORD(timeout.milliseconds);
		auto wakeup = WaitForSingleObject(overlapped.hEvent, milliseconds);

		if (wakeup == WAIT_TIMEOUT)
		{
			CancelIo(self->winos_named_pipe);
			return IO_ERROR_TIMEOUT;
		}
		CloseHandle(overlapped.hEvent);
		return overlapped.InternalHigh;
	}

	bool
	local_socket_disconnect(Local_Socket self)
	{
		worker_block_ahead();
		FlushFileBuffers((HANDLE)self->winos_named_pipe);
		auto res = DisconnectNamedPipe((HANDLE)self->winos_named_pipe);
		worker_block_clear();
		return res;
	}
}
