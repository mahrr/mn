#include <mn/IO.h>
#include <mn/File.h>
#include <mn/Defer.h>
#include <mn/Thread.h>
#include <mn/Path.h>
#include <mn/Assert.h>

int
main()
{
	auto f = mn::file_open("koko.bin", mn::IO_MODE_READ_WRITE, mn::OPEN_MODE_CREATE_OVERWRITE);
	mn_defer{mn::file_close(f);};

	uint64_t v = 0;
	mn::file_cursor_move_to_start(f);
	mn::file_write(f, mn::block_from(v), mn::INFINITE_TIMEOUT);

	while(true)
	{
		mn::file_cursor_move_to_start(f);

		mn::file_write_lock(f, 0, 8);

		auto res = mn::file_read(f, mn::block_from(v), mn::INFINITE_TIMEOUT);
		mn_assert(res.err == mn::IO_ERROR_NONE);
		mn_assert(res.val == 8);
		++v;
		mn::file_cursor_move_to_start(f);
		res = mn::file_write(f, mn::block_from(v), mn::INFINITE_TIMEOUT);
		mn_assert(res.err == mn::IO_ERROR_NONE);
		mn_assert(res.val == 8);

		mn::file_write_unlock(f, 0, 8);

		mn::print("Version '{}'\n", v);
		mn::thread_sleep(1000);
	}

	return 0;
}