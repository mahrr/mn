#include <mn/IO.h>
#include <mn/IPC.h>
#include <mn/Defer.h>
#include <mn/Assert.h>

void
byte_client(mn::ipc::Local_Socket client, mn::Str& line)
{
	auto [write_bytes, write_err] = mn::ipc::local_socket_write(client, mn::block_from(line));
	if (write_err)
	{
		mn::print("{}\n", mn::io_error_message(write_err));
		return;
	}
	mn_assert_msg(write_bytes == line.count, "local_socket_write failed");

	mn::str_resize(line, 1024);
	auto [read_bytes, read_err] = mn::ipc::local_socket_read(client, mn::block_from(line), mn::INFINITE_TIMEOUT);
	if (read_err)
	{
		mn::print("{}\n", mn::io_error_message(read_err));
		return;
	}
	mn_assert(read_bytes == write_bytes);

	mn::str_resize(line, read_bytes);
	mn::print("server: '{}'\n", line);
}

int
main()
{
	auto client = mn::ipc::local_socket_connect("sputnik");
	mn_assert_msg(client, "local_socket_connect failed");
	mn_defer{mn::ipc::local_socket_free(client);};

	auto line = mn::str_new();
	mn_defer{mn::str_free(line);};
	size_t read_bytes = 0, write_bytes = 0;
	do
	{
		mn::readln(line);
		if(line == "quit")
			break;
		else if(line.count == 0)
			continue;

		mn::print("you write: '{}'\n", line);

		// client byte stream
		byte_client(client, line);
		read_bytes = 1;
	} while(read_bytes > 0);

	return 0;
}