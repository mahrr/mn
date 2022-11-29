#include <mn/IO.h>
#include <mn/Fabric.h>
#include <mn/IPC.h>
#include <mn/Defer.h>
#include <mn/Assert.h>

void
serve_client(mn::ipc::Local_Socket client)
{
	auto data = mn::str_new();
	mn_defer
	{
		mn::str_free(data);
		mn::ipc::local_socket_free(client);
	};
	size_t read_bytes = 0;

	do
	{
		mn::str_resize(data, 1024);
		auto [read_bytes_count, read_err] = mn::ipc::local_socket_read(client, mn::block_from(data), mn::INFINITE_TIMEOUT);
		if (read_err)
		{
			mn::print("{}\n", mn::io_error_message(read_err));
			return;
		}
		read_bytes = read_bytes_count;

		if(read_bytes > 0)
		{
			mn::str_resize(data, read_bytes);
			auto [write_bytes, write_err] = mn::ipc::local_socket_write(client, mn::block_from(data));
			if (write_err)
			{
				mn::print("{}\n", mn::io_error_message(write_err));
				return;
			}
			mn_assert_msg(write_bytes == read_bytes, "local_socket_write failed");
		}
	} while(read_bytes > 0);
}

int
main()
{
	auto f = mn::fabric_new({});
	mn_defer{mn::fabric_free(f);};

	auto server = mn::ipc::local_socket_new("sputnik");
	mn_assert_msg(server, "local_socket_new failed");
	mn_defer{
		mn::ipc::local_socket_disconnect(server);
		mn::ipc::local_socket_free(server);
	};

	while(mn::ipc::local_socket_listen(server))
	{
		auto client = mn::ipc::local_socket_accept(server, { 10000 });
		if (client)
		{
			mn::go(f, serve_client, client);
		}
		else
		{
			mn::print("accept timed out, trying again\n");
		}
	}
	return 0;
}