#include <mn/IO.h>
#include <mn/Fabric.h>
#include <mn/IPC.h>
#include <mn/Log.h>

#include <fmt/chrono.h>

#include <chrono>

constexpr const char* ADDRESS = "benchmark";
constexpr size_t MSG_SIZE = 256 * 1024;
constexpr size_t PINGS_COUNT = 10000;

inline static void
server()
{
	auto socket = mn::ipc::local_socket_new(ADDRESS);
	if (socket == nullptr)
	{
		mn::log_critical("failed to create local socket '{}'", ADDRESS);
		return;
	}
	mn_defer
	{
		mn::ipc::local_socket_disconnect(socket);
		mn::ipc::local_socket_free(socket);
	};

	if (mn::ipc::local_socket_listen(socket) == false)
	{
		mn::log_critical("failed to listen to server socket '{}'", ADDRESS);
		return;
	}

	auto client = mn::ipc::local_socket_accept(socket, mn::INFINITE_TIMEOUT);
	if (client == nullptr)
	{
		mn::log_critical("failed to accept client socket '{}'", ADDRESS);
		return;
	}
	mn_defer
	{
		mn::ipc::local_socket_disconnect(client);
		mn::ipc::local_socket_free(client);
	};

	auto buf = mn::alloc(MSG_SIZE, alignof(char));
	mn_defer { mn::free(buf); };
	for (size_t i = 0; i < PINGS_COUNT; ++i)
	{
		auto [nread, read_err] = mn::ipc::local_socket_read(client, buf, mn::INFINITE_TIMEOUT);
		if (read_err)
		{
			mn::log_critical("failed to read ping from client, {}", mn::io_error_message(read_err));
			return;
		}
		if (nread != MSG_SIZE)
		{
			mn::log_critical("bad nread = {}", nread);
			return;
		}
	}
}

int main()
{
	auto f = mn::fabric_new({});
	mn_defer { mn::fabric_free(f); };

	mn::go(f, server);

	mn::thread_sleep(50);

	auto client = mn::ipc::local_socket_connect(ADDRESS);
	if (client == nullptr)
	{
		mn::log_critical("failed to connect to server");
		return -1;
	}
	mn_defer { mn::ipc::local_socket_free(client); };

	auto buf = mn::alloc(MSG_SIZE, alignof(char));
	mn_defer { mn::free(buf); };
	auto t1 = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < PINGS_COUNT; ++i)
	{
		auto [nwrite, write_err] = mn::ipc::local_socket_write(client, buf, mn::INFINITE_TIMEOUT);
		if (write_err)
		{
			mn::log_critical("failed to write ping from server, {}", mn::io_error_message(write_err));
			return -1;
		}
		if (nwrite != MSG_SIZE)
		{
			mn::log_critical("bad nwrite = {}", nwrite);
			return -1;
		}
	}
	auto t2 = std::chrono::high_resolution_clock::now();

	auto total_data = PINGS_COUNT * MSG_SIZE;
	mn::print("Client done\n");
	mn::print(
		"Sent {} msg in {} ns; throughput {} msg/sec ({} MB/sec)\n",
		PINGS_COUNT,
		t2 - t1,
		(MSG_SIZE * 1000'000'000) / (t2 - t1).count(),
		(total_data * 1000)/(t2 - t1).count()
	);
	mn::thread_sleep(50);

	return 0;
}