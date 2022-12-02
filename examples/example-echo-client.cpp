#include <mn/IO.h>
#include <mn/Socket.h>
#include <mn/Defer.h>
#include <mn/Assert.h>

int
main()
{
	auto socket = mn::socket_open(mn::SOCKET_FAMILY_IPV4, mn::SOCKET_TYPE_TCP);
	mn_assert_msg(socket, "socket_open failed");
	mn_defer{mn::socket_close(socket);};

	bool status = mn::socket_connect(socket, "localhost", "4000");
	mn_assert_msg(status, "socket_connect failed");
	mn_defer{mn::socket_disconnect(socket);};

	auto line = mn::str_new();
	mn_defer{mn::str_free(line);};
	size_t read_bytes = 0;
	do
	{
		mn::readln(line);
		if(line == "quit")
			break;
		else if(line.count == 0)
			continue;

		mn::print("you write: '{}'\n", line);

		auto [written_bytes, write_err] = mn::socket_write(socket, mn::block_from(line), mn::INFINITE_TIMEOUT);
		if (write_err != mn::IO_ERROR_NONE)
		{
			mn::print("{}\n", mn::io_error_message(write_err));
			break;
		}
		mn_assert(write_err == mn::IO_ERROR_NONE);
		mn_assert_msg(written_bytes == line.count, "socket_write failed");

		mn::str_resize(line, 1024);
		auto [read_bytes_count, read_err] = socket_read(socket, mn::block_from(line), mn::INFINITE_TIMEOUT);
		if (read_err)
		{
			mn::print("{}\n", mn::io_error_message(read_err));
			break;
		}
		read_bytes = read_bytes_count;
		mn_assert(read_bytes == written_bytes);

		mn::str_resize(line, read_bytes);
		mn::print("server: '{}'\n", line);
	} while(read_bytes > 0);

	return 0;
}