#include <mn/IO.h>
#include <mn/Fabric.h>
#include <mn/Socket.h>
#include <mn/Log.h>

#include <fmt/chrono.h>

#include <chrono>

constexpr const char* ADDRESS = "4000";
constexpr size_t MSG_SIZE = 256 * 1024;
constexpr size_t PINGS_COUNT = 10000;

inline static void
server()
{
	auto socket = mn::socket_open(mn::SOCKET_FAMILY_IPV4, mn::SOCKET_TYPE_TCP);
	if (socket == nullptr)
	{
		mn::log_critical("failed to create socket '{}'", ADDRESS);
		return;
	}
	mn_defer { mn::socket_close(socket); };

	if (mn::socket_bind(socket, ADDRESS) == false)
	{
		mn::log_critical("failed to bind to server socket '{}'", ADDRESS);
		return;
	}
	mn_defer { mn::socket_disconnect(socket); };

	if (mn::socket_listen(socket) == false)
	{
		mn::log_critical("failed to listen to server socket '{}'", ADDRESS);
		return;
	}

	auto client = mn::socket_accept(socket, mn::INFINITE_TIMEOUT);
	if (client == nullptr)
	{
		mn::log_critical("failed to accept client socket '{}'", ADDRESS);
		return;
	}
	mn_defer
	{
		mn::socket_disconnect(client);
		mn::socket_close(client);
	};

	auto buf = mn::alloc(MSG_SIZE, alignof(char));
	mn_defer { mn::free(buf); };
	for (size_t i = 0; i < PINGS_COUNT; ++i)
	{
		auto [nread, read_err] = mn::stream_copy(buf, client);
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

	auto client = mn::socket_open(mn::SOCKET_FAMILY_IPV4, mn::SOCKET_TYPE_TCP);
	if (client == nullptr)
	{
		mn::log_critical("failed to open to server");
		return -1;
	}
	mn_defer { mn::socket_close(client); };

	if (mn::socket_connect(client, "localhost", ADDRESS) == false)
	{
		mn::log_critical("failed to bind to server socket '{}'", ADDRESS);
		return -1;
	}
	mn_defer { mn::socket_disconnect(client); };

	auto buf = mn::alloc(MSG_SIZE, alignof(char));
	mn_defer { mn::free(buf); };
	auto t1 = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < PINGS_COUNT; ++i)
	{
		auto [nwrite, write_err] = mn::socket_write(client, buf, mn::INFINITE_TIMEOUT);
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