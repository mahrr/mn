#include <mn/IO.h>
#include <mn/File.h>
#include <mn/Defer.h>
#include <mn/Thread.h>
#include <mn/Path.h>
#include <mn/Assert.h>

int
main()
{
	auto o = mn::file_open("other.bin", mn::IO_MODE_READ_WRITE, mn::OPEN_MODE_CREATE_OVERWRITE);

	auto f = mn::file_open("koko.bin", mn::IO_MODE_READ_WRITE, mn::OPEN_MODE_OPEN_ONLY);

	mn::file_write_lock(f, 0, 8);

	uint64_t v = 0;
	auto res = mn::file_read(f, mn::block_from(v), mn::INFINITE_TIMEOUT);
	mn_assert(res.val == 8);

	mn::file_write_unlock(f, 0, 8);

	mn::print("Version '{}'\n", v);

	v = 0;
	mn::file_write(o, mn::block_from(v), mn::INFINITE_TIMEOUT);

	mn::file_remove("koko.bin");
	mn::file_move("other.bin", "koko.bin");

	mn::file_close(o);
	mn::file_close(f);
	return 0;
}