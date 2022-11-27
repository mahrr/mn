#include <mn/IO.h>
#include <mn/Fabric.h>
#include <mn/Socket.h>
#include <mn/Defer.h>
#include <mn/Assert.h>

void
serve_client(mn::Socket client)
{
	auto data = mn::str_new();
	mn_defer
	{
		mn::str_free(data);
		mn::socket_close(client);
	};
	size_t read_bytes = 0;

	do
	{
		mn::str_resize(data, 1024);
		auto [read_bytes_count, read_err] = mn::socket_read(client, mn::block_from(data), mn::INFINITE_TIMEOUT);
		if (read_err != mn::IO_ERROR_NONE)
		{
			mn::print("{}\n", mn::io_error_message(read_err));
			break;
		}
		read_bytes = read_bytes_count;

		if(read_bytes > 0)
		{
			mn::str_resize(data, read_bytes);
			auto [written_bytes, write_err] = mn::socket_write(client, mn::block_from(data));
			if (write_err != mn::IO_ERROR_NONE)
			{
				mn::print("{}\n", mn::io_error_message(write_err));
				break;
			}
			mn_assert_msg(written_bytes == read_bytes, "socket_write failed");
		}
		else if (read_bytes == 0)
		{
			mn::print("Read timeout\n");
		}
	} while(read_bytes > 0);
}

int
main()
{
	auto f = mn::fabric_new({});
	mn_defer{mn::fabric_free(f);};

	auto socket = mn::socket_open(mn::SOCKET_FAMILY_IPV4, mn::SOCKET_TYPE_TCP);
	mn_assert_msg(socket, "socket_open failed");
	mn_defer{mn::socket_close(socket);};

	bool status = mn::socket_bind(socket, "4000");
	mn_assert_msg(status, "socket_bind failed");
	mn_defer{mn::socket_disconnect(socket);};

	while(socket_listen(socket))
	{
		auto client_socket = mn::socket_accept(socket, { 1000 });
		if (client_socket)
			mn::go(f, serve_client, client_socket);
		else
			mn::print("socket accept timed out, trying again\n");
	}
	return 0;
}