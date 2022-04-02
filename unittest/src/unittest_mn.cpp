#include <doctest/doctest.h>

#include <mn/Memory.h>
#include <mn/Buf.h>
#include <mn/Str.h>
#include <mn/Map.h>
#include <mn/Pool.h>
#include <mn/Memory_Stream.h>
#include <mn/Virtual_Memory.h>
#include <mn/IO.h>
#include <mn/Str_Intern.h>
#include <mn/Ring.h>
#include <mn/OS.h>
#include <mn/memory/Leak.h>
#include <mn/Task.h>
#include <mn/Path.h>
#include <mn/Fmt.h>
#include <mn/Defer.h>
#include <mn/Deque.h>
#include <mn/Result.h>
#include <mn/Fabric.h>
#include <mn/Block_Stream.h>
#include <mn/Handle_Table.h>
#include <mn/UUID.h>
#include <mn/SIMD.h>
#include <mn/Json.h>
#include <mn/Regex.h>
#include <mn/Log.h>

#include <chrono>
#include <iostream>
#include <sstream>

#define ANKERL_NANOBENCH_IMPLEMENT 1
#include <nanobench.h>

TEST_CASE("allocation")
{
	auto b = mn::alloc(sizeof(int), alignof(int));
	CHECK(b.ptr != nullptr);
	CHECK(b.size != 0);

	free(b);
}

TEST_CASE("stack allocator")
{
	auto stack = mn::allocator_stack_new(1024);

	mn::allocator_push(stack);
	CHECK(mn::allocator_top() == stack);

	auto b = mn::alloc(512, alignof(char));
	mn::free(b);

	mn::allocator_pop();

	mn::allocator_free(stack);
}

TEST_CASE("arena allocator")
{
	auto arena = mn::allocator_arena_new(512);

	mn::allocator_push(arena);
	CHECK(mn::allocator_top() == arena);

	for (int i = 0; i < 1000; ++i)
		mn::alloc<int>();

	mn::allocator_pop();

	mn::allocator_free(arena);
}

TEST_CASE("tmp allocator")
{
	{
		auto name = mn::str_with_allocator(mn::memory::tmp());
		name = mn::strf(name, "Name: {}", "Mostafa");
		CHECK(name == "Name: Mostafa");
	}

	mn::memory::tmp()->free_all();

	{
		auto name = mn::str_with_allocator(mn::memory::tmp());
		name = mn::strf(name, "Name: {}", "Mostafa");
		CHECK(name == "Name: Mostafa");
	}

	mn::memory::tmp()->free_all();
}

TEST_CASE("buf push")
{
	auto arr = mn::buf_new<int>();
	for (int i = 0; i < 10; ++i)
		mn::buf_push(arr, i);
	for (size_t i = 0; i < arr.count; ++i)
		CHECK(i == arr[i]);
	mn::buf_free(arr);
}

TEST_CASE("buf insert and remove ordered")
{
	auto v = mn::buf_lit({1, 2, 3, 5});
	mn::buf_insert(v, 3, 4);
	mn::buf_insert(v, 5, 6);
	for(size_t i = 0; i < v.count; ++i)
		CHECK(v[i] == i + 1);
	mn::buf_remove_ordered(v, 3);
	CHECK(v.count == 5);
	CHECK(v[0] == 1);
	CHECK(v[1] == 2);
	CHECK(v[2] == 3);
	CHECK(v[3] == 5);
	CHECK(v[4] == 6);
	mn::buf_free(v);
}

TEST_CASE("range for loop")
{
	auto arr = mn::buf_new<int>();
	for (int i = 0; i < 10; ++i)
		mn::buf_push(arr, i);
	size_t i = 0;
	for (auto num : arr)
		CHECK(num == i++);
	mn::buf_free(arr);
}

TEST_CASE("buf pop")
{
	auto arr = mn::buf_new<int>();
	for (int i = 0; i < 10; ++i)
		mn::buf_push(arr, i);
	CHECK(mn::buf_empty(arr) == false);
	for (size_t i = 0; i < 10; ++i)
		mn::buf_pop(arr);
	CHECK(mn::buf_empty(arr) == true);
	mn::buf_free(arr);
}

TEST_CASE("str push")
{
	auto str = mn::str_new();

	mn::str_push(str, "Mostafa");
	CHECK("Mostafa" == str);

	mn::str_push(str, " Saad");
	CHECK(str == "Mostafa Saad");

	mn::str_push(str, " Abdel-Hameed");
	CHECK(str == "Mostafa Saad Abdel-Hameed");

	str = mn::strf(str, " age: {}", 25);
	CHECK(str == "Mostafa Saad Abdel-Hameed age: 25");

	auto new_str = mn::str_new();
	for(const char* it = begin(str); it != end(str); it = mn::rune_next(it))
	{
		auto r = mn::rune_read(it);
		mn::str_push(new_str, r);
	}
	CHECK(new_str == str);

	mn::str_free(new_str);
	mn::str_free(str);
}

TEST_CASE("str null terminate")
{
	auto str = mn::str_new();
	mn::str_null_terminate(str);
	CHECK(str == "");
	CHECK(str.count == 0);

	mn::buf_pushn(str, 5, 'a');
	mn::str_null_terminate(str);
	CHECK(str == "aaaaa");
	mn::str_free(str);
}

TEST_CASE("str find")
{
	CHECK(mn::str_find("hello world", "hello world", 0) == 0);
	CHECK(mn::str_find("hello world", "hello", 0) == 0);
	CHECK(mn::str_find("hello world", "hello", 1) == SIZE_MAX);
	CHECK(mn::str_find("hello world", "world", 0) == 6);
	CHECK(mn::str_find("hello world", "ld", 0) == 9);
	CHECK(mn::str_find("hello world", "hello", 8) == SIZE_MAX);
	CHECK(mn::str_find("hello world", "hello world hello", 0) == SIZE_MAX);
	CHECK(mn::str_find("hello world", "", 0) == 0);
	CHECK(mn::str_find("", "hello", 0) == SIZE_MAX);
}

TEST_CASE("str find benchmark")
{
	auto source = mn::str_tmpf("hello 0");
	for (size_t i = 0; i < 100; ++i)
		source = mn::strf(source, ", hello {}", i + 1);
	ankerl::nanobench::Bench().minEpochIterations(233).run("small find", [&]{
		auto res = mn::str_find(source, "world", 0);
		ankerl::nanobench::doNotOptimizeAway(res);
	});
}

TEST_CASE("str find last")
{
	CHECK(mn::str_find_last("hello world", "hello world", 11) == 0);
	CHECK(mn::str_find_last("hello world", "hello world", 0) == SIZE_MAX);
	CHECK(mn::str_find_last("hello world", "world", 9) == SIZE_MAX);
	CHECK(mn::str_find_last("hello world", "world", 11) == 6);
	CHECK(mn::str_find_last("hello world", "ld", 11) == 9);
	CHECK(mn::str_find_last("hello world", "hello", 8) == 0);
	CHECK(mn::str_find_last("hello world", "world", 3) == SIZE_MAX);
	CHECK(mn::str_find_last("hello world", "hello world hello", 11) == SIZE_MAX);
	CHECK(mn::str_find_last("hello world", "", 11) == 11);
	CHECK(mn::str_find_last("", "hello", 11) == SIZE_MAX);
}

TEST_CASE("str find last benchmark")
{
	auto source = mn::str_tmpf("hello 0");
	for (size_t i = 0; i < 100; ++i)
		source = mn::strf(source, ", hello {}", i + 1);
	ankerl::nanobench::Bench().minEpochIterations(233).run("small find last", [&]{
		auto res = mn::str_find_last(source, "hello 0", source.count);
		ankerl::nanobench::doNotOptimizeAway(res);
	});
}

TEST_CASE("str split")
{
	auto res = mn::str_split(",A,B,C,", ",", true);
	CHECK(res.count == 3);
	CHECK(res[0] == "A");
	CHECK(res[1] == "B");
	CHECK(res[2] == "C");
	destruct(res);

	res = mn::str_split("A,B,C", ",", false);
	CHECK(res.count == 3);
	CHECK(res[0] == "A");
	CHECK(res[1] == "B");
	CHECK(res[2] == "C");
	destruct(res);

	res = mn::str_split(",A,B,C,", ",", false);
	CHECK(res.count == 5);
	CHECK(res[0] == "");
	CHECK(res[1] == "A");
	CHECK(res[2] == "B");
	CHECK(res[3] == "C");
	CHECK(res[4] == "");
	destruct(res);

	res = mn::str_split("A", ";;;", true);
	CHECK(res.count == 1);
	CHECK(res[0] == "A");
	destruct(res);

	res = mn::str_split("", ",", false);
	CHECK(res.count == 1);
	CHECK(res[0] == "");
	destruct(res);

	res = mn::str_split("", ",", true);
	CHECK(res.count == 0);
	destruct(res);

	res = mn::str_split(",,,,,", ",", true);
	CHECK(res.count == 0);
	destruct(res);

	res = mn::str_split(",,,", ",", false);
	CHECK(res.count == 4);
	CHECK(res[0] == "");
	CHECK(res[1] == "");
	CHECK(res[2] == "");
	CHECK(res[3] == "");
	destruct(res);

	res = mn::str_split(",,,", ",,", false);
	CHECK(res.count == 2);
	CHECK(res[0] == "");
	CHECK(res[1] == ",");
	destruct(res);

	res = mn::str_split("test", ",,,,,,,,", false);
	CHECK(res.count == 1);
	CHECK(res[0] == "test");
	destruct(res);

	res = mn::str_split("test", ",,,,,,,,", true);
	CHECK(res.count == 1);
	CHECK(res[0] == "test");
	destruct(res);
}

TEST_CASE("str trim")
{
	auto s = mn::str_from_c("     \r\ntrim  \v");
	mn::str_trim(s);
	CHECK(s == "trim");
	mn::str_free(s);

	s = mn::str_from_c("     \r\ntrim \n koko \v");
	mn::str_trim(s);
	CHECK(s == "trim \n koko");
	mn::str_free(s);

	s = mn::str_from_c("r");
	mn::str_trim(s);
	CHECK(s == "r");
	mn::str_free(s);

	s = mn::str_from_c("ab");
	mn::str_trim(s, "b");
	CHECK(s == "a");
	mn::str_free(s);
}

TEST_CASE("String lower case and upper case")
{
	auto word = mn::str_from_c("مصطفى");
	mn::str_lower(word);
	CHECK(word == "مصطفى");
	mn::str_free(word);

	auto word2 = mn::str_from_c("PERCHÉa");
	mn::str_lower(word2);
	CHECK(word2 == "perchéa");
	mn::str_free(word2);

	auto word3 = mn::str_from_c("Æble");
	mn::str_lower(word3);
	CHECK(word3 == "æble");
	mn::str_free(word3);
}

TEST_CASE("set general cases")
{
	auto num = mn::set_new<int>();

	for (int i = 0; i < 10; ++i)
		mn::set_insert(num, i);

	for (int i = 0; i < 10; ++i)
	{
		CHECK(*mn::set_lookup(num, i) == i);
	}

	for (int i = 10; i < 20; ++i)
		CHECK(mn::set_lookup(num, i) == nullptr);

	for (int i = 0; i < 10; ++i)
	{
		if (i % 2 == 0)
			mn::set_remove(num, i);
	}

	for (int i = 0; i < 10; ++i)
	{
		if (i % 2 == 0)
		{
			CHECK(mn::set_lookup(num, i) == nullptr);
		}
		else
		{
			CHECK(*mn::set_lookup(num, i) == i);
		}
	}

	int i = 0;
	for (auto n: num)
		++i;
	CHECK(i == 5);

	mn::set_free(num);
}

TEST_CASE("map general cases")
{
	auto num = mn::map_new<int, int>();

	for (int i = 0; i < 10; ++i)
		mn::map_insert(num, i, i + 10);

	for (int i = 0; i < 10; ++i)
	{
		CHECK(mn::map_lookup(num, i)->key == i);
		CHECK(mn::map_lookup(num, i)->value == i + 10);
	}

	for (int i = 10; i < 20; ++i)
		CHECK(mn::map_lookup(num, i) == nullptr);

	for (int i = 0; i < 10; ++i)
	{
		if (i % 2 == 0)
			mn::map_remove(num, i);
	}

	for (int i = 0; i < 10; ++i)
	{
		if (i % 2 == 0)
			CHECK(mn::map_lookup(num, i) == nullptr);
		else
		{
			CHECK(mn::map_lookup(num, i)->key == i);
			CHECK(mn::map_lookup(num, i)->value == i + 10);
		}
	}

	int i = 0;
	for(const auto& [key, value]: num)
		++i;
	CHECK(i == 5);

	mn::map_free(num);
}

TEST_CASE("Pool general case")
{
	auto pool = mn::pool_new(sizeof(int), 1024);
	int* ptr = (int*)mn::pool_get(pool);
	CHECK(ptr != nullptr);
	*ptr = 234;
	mn::pool_put(pool, ptr);
	int* new_ptr = (int*)mn::pool_get(pool);
	CHECK(new_ptr == ptr);

	int* new_ptr2 = (int*)mn::pool_get(pool);
	mn::pool_put(pool, new_ptr2);

	mn::pool_put(pool, new_ptr);
	mn::pool_free(pool);
}

TEST_CASE("Memory_Stream general case")
{
	auto mem = mn::memory_stream_new();
	CHECK(mn::memory_stream_size(mem) == 0);
	CHECK(mn::memory_stream_cursor_pos(mem) == 0);
	mn::memory_stream_write(mem, mn::block_lit("Mostafa"));
	CHECK(mn::memory_stream_size(mem) == 7);
	CHECK(mn::memory_stream_cursor_pos(mem) == 7);

	char name[8] = { 0 };
	CHECK(mn::memory_stream_read(mem, mn::block_from(name)) == 0);
	CHECK(mn::memory_stream_cursor_pos(mem) == 7);

	mn::memory_stream_cursor_to_start(mem);
	CHECK(mn::memory_stream_cursor_pos(mem) == 0);

	CHECK(mn::memory_stream_read(mem, mn::block_from(name)) == 7);
	CHECK(mn::memory_stream_cursor_pos(mem) == 7);

	CHECK(::strcmp(name, "Mostafa") == 0);
	mn::memory_stream_free(mem);
}

TEST_CASE("virtual memory allocation")
{
	size_t size = 1ULL * 1024ULL * 1024ULL * 1024ULL;
	auto block = mn::virtual_alloc(nullptr, size);
	CHECK(block.ptr != nullptr);
	CHECK(block.size == size);
	mn::virtual_free(block);
}

TEST_CASE("reads")
{
	int a, b;
	float c, d;
	auto e = mn::str_new();
	size_t read_count = mn::reads("-123 20 1.23 0.123 Mostafa ", a, b, c, d, e);
	CHECK(read_count == 5);
	CHECK(a == -123);
	CHECK(b == 20);
	CHECK(c == 1.23f);
	CHECK(d == 0.123f);
	CHECK(e == "Mostafa");
	mn::str_free(e);
}

TEST_CASE("reader")
{
	auto reader = mn::reader_wrap_str(nullptr, "Mostafa Saad");
	auto str = mn::str_new();
	size_t read_count = mn::readln(reader, str);
	CHECK(read_count == 12);
	CHECK(str == "Mostafa Saad");

	mn::str_free(str);
	mn::reader_free(reader);
}

TEST_CASE("reader with empty newline")
{
	auto text = R"""(my name is mostafa

mostafa is 26 years old)""";
	auto reader = mn::reader_wrap_str(nullptr, text);
	auto str = mn::str_new();

	size_t read_count = mn::readln(reader, str);
	CHECK(read_count == 19);
	CHECK(str == "my name is mostafa");

	read_count = mn::readln(reader, str);
	CHECK(read_count == 1);
	CHECK(str == "");

	read_count = mn::readln(reader, str);
	CHECK(read_count == 23);
	CHECK(str == "mostafa is 26 years old");

	mn::str_free(str);
	mn::reader_free(reader);
}

TEST_CASE("path windows os encoding")
{
	auto os_path = mn::path_os_encoding("C:/bin/my_file.exe");

	#if OS_WINDOWS
		CHECK(os_path == "C:\\bin\\my_file.exe");
	#endif

	mn::str_free(os_path);
}

TEST_CASE("Str_Intern general case")
{
	auto intern = mn::str_intern_new();

	const char* is = mn::str_intern(intern, "Mostafa");
	CHECK(is != nullptr);
	CHECK(is == mn::str_intern(intern, "Mostafa"));

	const char* big_str = "my name is Mostafa";
	const char* begin = big_str + 11;
	const char* end = begin + 7;
	CHECK(is == mn::str_intern(intern, begin, end));

	mn::str_intern_free(intern);
}

TEST_CASE("simple data ring case")
{
	mn::allocator_push(mn::memory::leak());

	auto r = mn::ring_new<int>();

	for (int i = 0; i < 10; ++i)
		mn::ring_push_back(r, i);

	for (size_t i = 0; i < r.count; ++i)
		CHECK(r[i] == i);

	for (int i = 0; i < 10; ++i)
		mn::ring_push_front(r, i);

	for (int i = 9; i >= 0; --i)
	{
		CHECK(mn::ring_back(r) == i);
		mn::ring_pop_back(r);
	}

	for (int i = 9; i >= 0; --i)
	{
		CHECK(mn::ring_front(r) == i);
		mn::ring_pop_front(r);
	}

	mn::ring_free(r);

	mn::allocator_pop();
}

TEST_CASE("complex data ring case")
{
	mn::allocator_push(mn::memory::leak());
	auto r = mn::ring_new<mn::Str>();

	for (int i = 0; i < 10; ++i)
		mn::ring_push_back(r, mn::str_from_c("Mostafa"));

	for (int i = 0; i < 10; ++i)
		mn::ring_push_front(r, mn::str_from_c("Saad"));

	for (int i = 4; i >= 0; --i)
	{
		CHECK(mn::ring_back(r) == "Mostafa");
		mn::str_free(mn::ring_back(r));
		mn::ring_pop_back(r);
	}

	for (int i = 4; i >= 0; --i)
	{
		CHECK(mn::ring_front(r) == "Saad");
		mn::str_free(mn::ring_front(r));
		mn::ring_pop_front(r);
	}

	destruct(r);

	mn::allocator_pop();
}

TEST_CASE("Rune")
{
	CHECK(mn::rune_upper('a') == 'A');
	CHECK(mn::rune_upper('A') == 'A');
	CHECK(mn::rune_lower('A') == 'a');
	CHECK(mn::rune_lower('a') == 'a');
	CHECK(mn::rune_lower(U'\u0645') == U'\u0645');
}

TEST_CASE("Task")
{
	CHECK(std::is_pod_v<mn::Task<void()>> == true);

	auto add = mn::Task<int(int, int)>::make([](int a, int b) { return a + b; });
	CHECK(std::is_pod_v<decltype(add)> == true);

	auto inc = mn::Task<int(int)>::make([=](int a) mutable { return add(a, 1); });
	CHECK(std::is_pod_v<decltype(inc)> == true);

	CHECK(add(1, 2) == 3);
	CHECK(inc(5) == 6);

	mn::task_free(add);
	mn::task_free(inc);
}

struct V2
{
	int x, y;
};

inline static std::ostream&
operator<<(std::ostream& out, const V2& v)
{
	out << "V2{ " << v.x << ", " << v.y << " }";
	return out;
}

TEST_CASE("Fmt")
{
	SUBCASE("str formatting")
	{
		auto n = mn::strf("{}", "mostafa"_mnstr);
		CHECK(n == "mostafa");
		mn::str_free(n);
	}

	SUBCASE("buf formatting")
	{
		auto b = mn::buf_lit({1, 2, 3});
		auto n = mn::strf("{}", b);
		CHECK(n == "[3]{0: 1, 1: 2, 2: 3 }");
		mn::str_free(n);
		mn::buf_free(b);
	}

	SUBCASE("map formatting")
	{
		auto m = mn::map_new<mn::Str, V2>();
		mn::map_insert(m, mn::str_from_c("ABC"), V2{654, 765});
		mn::map_insert(m, mn::str_from_c("DEF"), V2{6541, 7651});
		auto n = mn::strf("{}", m);
		CHECK(n == "[2]{ ABC: V2{ 654, 765 }, DEF: V2{ 6541, 7651 } }");
		mn::str_free(n);
		destruct(m);
	}
}

TEST_CASE("Deque")
{
	SUBCASE("empty deque")
	{
		auto n = mn::deque_new<int>();
		mn::deque_free(n);
	}

	SUBCASE("deque push")
	{
		auto nums = mn::deque_new<int>();
		for (int i = 0; i < 1000; ++i)
		{
			if (i % 2 == 0)
				mn::deque_push_front(nums, i);
			else
				mn::deque_push_back(nums, i);
		}

		for (int i = 0; i < 500; ++i)
			CHECK(nums[i] % 2 == 0);

		for (int i = 500; i < 1000; ++i)
			CHECK(nums[i] % 2 != 0);

		mn::deque_free(nums);
	}

	SUBCASE("deque pop")
	{
		auto nums = mn::deque_new<int>();
		for (int i = 0; i < 10; ++i)
		{
			if (i % 2 == 0)
				mn::deque_push_front(nums, i);
			else
				mn::deque_push_back(nums, i);
		}

		CHECK(mn::deque_front(nums) == 8);
		CHECK(mn::deque_back(nums) == 9);

		mn::deque_pop_front(nums);
		CHECK(mn::deque_front(nums) == 6);

		mn::deque_pop_back(nums);
		CHECK(mn::deque_back(nums) == 7);

		mn::deque_free(nums);
	}
}

mn::Result<int> my_div(int a, int b)
{
	if (b == 0)
		return mn::Err{ "can't calc '{}/{}' because b is 0", a, b };
	return a / b;
}

enum class Err_Code { OK, ZERO_DIV };

mn::Result<int, Err_Code> my_div2(int a, int b)
{
	if (b == 0)
		return Err_Code::ZERO_DIV;
	return a / b;
}

TEST_CASE("Result default error")
{
	SUBCASE("no err")
	{
		auto [r, err] = my_div(4, 2);
		CHECK(err == false);
		CHECK(r == 2);
	}

	SUBCASE("err")
	{
		auto [r, err] = my_div(4, 0);
		CHECK(err == true);
	}
}

TEST_CASE("Result error code")
{
	SUBCASE("no err")
	{
		auto [r, err] = my_div2(4, 2);
		CHECK(err == Err_Code::OK);
		CHECK(r == 2);
	}

	SUBCASE("err")
	{
		auto [r, err] = my_div2(4, 0);
		CHECK(err == Err_Code::ZERO_DIV);
	}
}

TEST_CASE("fabric simple creation")
{
	mn::Fabric_Settings settings{};
	settings.workers_count = 3;
	auto f = mn::fabric_new(settings);
	mn::fabric_free(f);
}

TEST_CASE("fabric simple function")
{
	mn::Fabric_Settings settings{};
	settings.workers_count = 3;
	auto f = mn::fabric_new(settings);

	int n = 0;

	mn::Auto_Waitgroup g;

	g.add(1);

	go(f, [&n, &g] { ++n; g.done(); });

	g.wait();
	CHECK(n == 1);

	mn::fabric_free(f);
}

TEST_CASE("unbuffered channel with multiple workers")
{
	mn::Fabric_Settings settings{};
	settings.workers_count = 3;
	auto f = mn::fabric_new(settings);
	auto c = mn::chan_new<size_t>();
	mn::Auto_Waitgroup g;

	std::atomic<size_t> sum = 0;

	auto worker = [c, &sum, &g]{
		for (auto& num : c)
			sum += num;
		g.done();
	};

	for(size_t i = 0; i < 3; ++i)
	{
		g.add(1);
		mn::go(f, worker);
	}

	for (size_t i = 0; i <= 100; ++i)
		mn::chan_send(c, i);
	mn::chan_close(c);

	g.wait();
	CHECK(sum == 5050);

	mn::chan_free(c);
	mn::fabric_free(f);
}

TEST_CASE("buffered channel")
{
	mn::Fabric_Settings settings{};
	settings.workers_count = 3;
	auto f = mn::fabric_new(settings);
	auto c = mn::chan_new<size_t>(1000);
	mn::Auto_Waitgroup g;

	std::atomic<size_t> sum = 0;

	auto worker = [c, &sum, &g] {
		for (const auto& num : c)
			sum += num;
		g.done();
	};

	for (size_t i = 0; i < 6; ++i)
	{
		g.add(1);
		mn::go(f, worker);
	}

	for (size_t i = 0; i <= 10000; ++i)
		mn::chan_send(c, i);
	mn::chan_close(c);

	g.wait();
	CHECK(sum == 50005000);

	mn::chan_free(c);
	mn::fabric_free(f);
}

TEST_CASE("unbuffered channel from coroutine")
{
	mn::Fabric_Settings settings{};
	settings.workers_count = 3;
	auto f = mn::fabric_new(settings);
	auto c = mn::chan_new<size_t>();
	mn::Auto_Waitgroup g;

	size_t sum = 0;

	g.add(1);
	mn::go(f, [c, &sum, &g] {
		for (auto num : c)
			sum += num;
		g.done();
	});

	mn::go(f, [c] {
		for (size_t i = 0; i <= 100; ++i)
			mn::chan_send(c, i);
		mn::chan_close(c);
	});

	g.wait();
	CHECK(sum == 5050);

	mn::fabric_free(f);
	mn::chan_free(c);
}

TEST_CASE("buffered channel from coroutine")
{
	mn::Fabric_Settings settings{};
	settings.workers_count = 3;
	auto f = mn::fabric_new(settings);
	auto c = mn::chan_new<size_t>(1000);
	mn::Auto_Waitgroup g;

	size_t sum = 0;

	g.add(1);
	mn::go(f, [c, &sum, &g] {
		for (auto num : c)
			sum += num;
		g.done();
	});

	mn::go(f, [c]{
		for (size_t i = 0; i <= 10000; ++i)
			mn::chan_send(c, i);
		mn::chan_close(c);
	});

	g.wait();
	CHECK(sum == 50005000);

	mn::fabric_free(f);
	mn::chan_free(c);
}

TEST_CASE("coroutine launching coroutines")
{
	mn::Fabric_Settings settings{};
	settings.workers_count = 3;
	auto f = mn::fabric_new(settings);
	auto c = mn::chan_new<int>(1000);
	mn::Auto_Waitgroup g;

	size_t sum = 0;

	g.add(1);
	mn::go(f, [&g, &sum, c]{

		mn::go([&g, &sum, c]{
			for (auto num : c)
				sum += num;
			g.done();
		});

		for (int i = 0; i <= 10000; ++i)
			mn::chan_send(c, i);
		mn::chan_close(c);
	});

	g.wait();
	CHECK(sum == 50005000);

	mn::fabric_free(f);
	mn::chan_free(c);
}

TEST_CASE("stress")
{
	auto f = mn::fabric_new({});
	auto c = mn::chan_new<size_t>(100);
	mn::Auto_Waitgroup g;

	std::atomic<size_t> sum = 0;

	for (size_t i = 0; i <= 1000; ++i)
	{
		g.add(1);
		mn::go(f, [c, i] { mn::chan_send(c, i); });
		mn::go(f, [c, &sum, &g] { auto[n, _] = mn::chan_recv(c); sum += n; g.done(); });
	}

	g.wait();
	CHECK(sum == 500500);

	mn::fabric_free(f);
	mn::chan_free(c);
}

TEST_CASE("future")
{
	auto f = mn::fabric_new({});

	auto async_int_func = [](int x){
		mn::thread_sleep(200);
		return x;
	};

	int global_var = 0;
	auto async_void_func = [&]() {
		mn::thread_sleep(200);
		global_var = 62;
	};

	auto fu = mn::future_go(f, async_int_func, 42);
	mn::future_wait(fu);
	CHECK(fu == 42);

	CHECK(mn::xchg(*fu, {}) == 42);
	CHECK(fu == 0);

	mn::future_free(fu);

	auto fu2 = mn::future_go(f, async_void_func);
	mn::future_wait(fu2);
	CHECK(global_var == 62);
	mn::future_free(fu2);

	mn::fabric_free(f);
}

TEST_CASE("buddy")
{
	auto buddy = mn::allocator_buddy_new();
	auto nums = mn::buf_with_allocator<int>(buddy);
	for(int i = 0; i < 1000; ++i)
		mn::buf_push(nums, i);
	auto test = mn::alloc_from(buddy, 1024*1024 - 16, alignof(int));
	CHECK(test.ptr == nullptr);
	for(int i = 0; i < 1000; ++i)
		CHECK(nums[i] == i);
	mn::buf_free(nums);
	mn::allocator_free(buddy);
}

TEST_CASE("handle table generation check")
{
	auto table = mn::handle_table_new<int>();
	auto handles = mn::buf_new<uint64_t>();
	for(int i = 0; i < 10; ++i)
		mn::buf_push(handles, mn::handle_table_insert(table, i));

	for(int i = 0; i < 10; ++i)
	{
		CHECK(mn::handle_table_get(table, handles[i]) == i);
		mn::handle_table_remove(table, handles[i]);
	}

	for(int i = 0; i < 10; ++i)
	{
		auto new_handle = mn::handle_table_insert(table, i);
		CHECK(new_handle != handles[i]);
	}

	mn::handle_table_free(table);
	mn::buf_free(handles);
}

TEST_CASE("handle table generation check")
{
	auto table = mn::handle_table_new<int>();
	auto handles = mn::buf_new<uint64_t>();
	for(int i = 0; i < 10; ++i)
		mn::buf_push(handles, mn::handle_table_insert(table, i));

	for(int i = 0; i < 10; ++i)
		if (i % 2 == 0)
			mn::handle_table_remove(table, handles[i]);

	for(size_t i = 0; i < handles.count; ++i)
	{
		if (handles[i] % 2 == 0)
		{
			mn::buf_remove(handles, i);
			--i;
		}
	}

	for(int i = 0; i < 10; ++i)
	{
		if (i % 2 == 0)
		{
			auto new_handle = mn::handle_table_insert(table, i);
			mn::buf_push(handles, new_handle);
		}
	}

	mn::handle_table_free(table);
	mn::buf_free(handles);
}

TEST_CASE("zero init buf")
{
	mn::Buf<int> nums{};
	for (int i = 0; i < 10; ++i)
		mn::buf_push(nums, i);
	for (int i = 0; i < 10; ++i)
		CHECK(nums[i] == i);
	mn::buf_free(nums);

	mn::Buf<int> nums2{};
	mn::buf_free(nums2);
}

TEST_CASE("zero init map")
{
	mn::Map<int, bool> table{};
	mn::map_insert(table, 1, true);
	CHECK(mn::map_lookup(table, 1)->value == true);
	mn::map_free(table);

}

TEST_CASE("uuid uniqueness")
{
	auto ids = mn::map_new<mn::UUID, size_t>();
	mn_defer{mn::map_free(ids);};

	for (size_t i = 0; i < 1000000; ++i)
	{
		auto id = mn::uuid_generate();
		if(auto it = mn::map_lookup(ids, id))
			++it->value;
		else
			mn::map_insert(ids, id, size_t(1));
	}
	CHECK(ids.count == 1000000);
}

TEST_CASE("uuid parsing")
{
	SUBCASE("Case 01")
	{
		auto id = mn::uuid_generate();
		auto variant = uuid_variant(id);
		auto version = uuid_version(id);
		auto id_str = mn::str_tmpf("{}", id);
		auto [id2, err] = mn::uuid_parse(id_str);
		CHECK(err == false);
		CHECK(id == id2);
		auto id2_str = mn::str_tmpf("{}", id2);
		CHECK(id2_str == id_str);
	}

	SUBCASE("Case 02")
	{
		auto [id, err] = mn::uuid_parse("this is not a uuid");
		CHECK(err == true);
	}

	SUBCASE("Case 03")
	{
		auto [id, err] = mn::uuid_parse("62013B88-FA54-4008-8D42-F9CA4889e0B5");
		CHECK(err == false);
	}

	SUBCASE("Case 04")
	{
		auto [id, err] = mn::uuid_parse("62013BX88-FA54-4008-8D42-F9CA4889e0B5");
		CHECK(err == true);
	}

	SUBCASE("Case 05")
	{
		auto [id, err] = mn::uuid_parse("{62013B88-FA54-4008-8D42-F9CA4889e0B5}");
		CHECK(err == false);
	}

	SUBCASE("Case 06")
	{
		auto [id, err] = mn::uuid_parse("62013B88,FA54-4008-8D42-F9CA4889e0B5");
		CHECK(err == true);
	}

	SUBCASE("Case 07")
	{
		auto [id, err] = mn::uuid_parse("62013B88-FA54-4008-8D42-F9CA4889e0B5AA");
		CHECK(err == true);
	}

	SUBCASE("Case 08")
	{
		auto nil_str = mn::str_tmpf("{}", mn::null_uuid);
		CHECK(nil_str == "00000000-0000-0000-0000-000000000000");
	}

	SUBCASE("Case 09")
	{
		auto [id, err] = mn::uuid_parse("00000000-0000-0000-0000-000000000000");
		CHECK(err == false);
		CHECK(id == mn::null_uuid);
	}
}

TEST_CASE("report simd")
{
	auto simd = mn_simd_support_check();
	mn::print("sse: {}\n", simd.sse_supportted);
	mn::print("sse2: {}\n", simd.sse2_supportted);
	mn::print("sse3: {}\n", simd.sse3_supportted);
	mn::print("sse4.1: {}\n", simd.sse4_1_supportted);
	mn::print("sse4.2: {}\n", simd.sse4_2_supportted);
	mn::print("sse4a: {}\n", simd.sse4a_supportted);
	mn::print("sse5: {}\n", simd.sse5_supportted);
	mn::print("avx: {}\n", simd.avx_supportted);
}

TEST_CASE("json support")
{
	auto json = R"""(
		{
			"name": "my name is \"mostafa\"",
			"x": null,
			"y": true,
			"z": false,
			"w": 213.123,
			"a": [
				1, false
			],
			"subobject": {
				"name": "subobject"
			}
		}
	)""";

	auto [v, err] = mn::json::parse(json);
	CHECK(err == false);
	auto v_str = mn::str_tmpf("{}", v);
	auto expected = R"""({"name":"my name is \"mostafa\"", "x":null, "y":true, "z":false, "w":213.123, "a":[1, false], "subobject":{"name":"subobject"}})""";
	CHECK(v_str == expected);
	mn::json::value_free(v);
}

inline static mn::Regex
compile(const char* str)
{
	auto [prog, err] = mn::regex_compile(str, mn::memory::tmp());
	CHECK(!err);
	return prog;
}

inline static bool
matched(const mn::Regex& program, const char* str)
{
	auto res = mn::regex_match(program, str);
	return res.match;
}

inline static bool
matched_substr(const mn::Regex& program, size_t count, const char* str)
{
	auto res = mn::regex_match(program, str);
	CHECK(res.end == (str + count));
	return res.match;
}

TEST_CASE("simple concat")
{
	auto prog = compile("abc");
	CHECK(matched(prog, "abc") == true);
	CHECK(matched(prog, "acb") == false);
	CHECK(matched(prog, "") == false);
}

TEST_CASE("simple or")
{
	auto prog = compile("ab(c|d)");
	CHECK(matched(prog, "abc") == true);
	CHECK(matched(prog, "abd") == true);
	CHECK(matched(prog, "ab") == false);
	CHECK(matched(prog, "") == false);
}

TEST_CASE("simple star")
{
	auto prog = compile("abc*");
	CHECK(matched(prog, "abc") == true);
	CHECK(matched(prog, "abd") == true);
	CHECK(matched(prog, "ab") == true);
	CHECK(matched_substr(prog, 9, "abccccccc") == true);
	CHECK(matched(prog, "") == false);
}

TEST_CASE("set star")
{
	auto prog = compile("[a-z]*");
	CHECK(matched(prog, "abc") == true);
	CHECK(matched(prog, "123") == true);
	CHECK(matched(prog, "ab") == true);
	CHECK(matched(prog, "DSFabccccccc") == true);
	CHECK(matched(prog, "") == true);
}

TEST_CASE("set plus")
{
	auto prog = compile("[a-z]+");
	CHECK(matched(prog, "abc") == true);
	CHECK(matched(prog, "123") == false);
	CHECK(matched(prog, "ab") == true);
	CHECK(matched(prog, "DSFabccccccc") == false);
	CHECK(matched(prog, "") == false);
}

TEST_CASE("C id")
{
	auto prog = compile("[a-zA-Z_][a-zA-Z0-9_]*");
	CHECK(matched(prog, "abc") == true);
	CHECK(matched(prog, "abc_def_123") == true);
	CHECK(matched(prog, "123") == false);
	CHECK(matched(prog, "ab") == true);
	CHECK(matched(prog, "DSFabccccccc") == true);
	CHECK(matched(prog, "") == false);
}

TEST_CASE("Email regex")
{
	auto prog = compile("[a-z0-9!#$%&'*+/=?^_`{|}~\\-]+(\\.[a-z0-9!#$%&'*+/=?^_`{|}~\\-]+)*@([a-z0-9]([a-z0-9\\-]*[a-z0-9])?\\.)+[a-z0-9]([a-z0-9\\-]*[a-z0-9])?");
	CHECK(matched(prog, "moustapha.saad.abdelhamed@gmail.com") == true);
	CHECK(matched(prog, "mostafa") == false);
	CHECK(matched(prog, "moustapha.saad.abdelhamed@gmail") == false);
	CHECK(matched(prog, "moustapha.saad.abdelhamed@.com") == false);
	CHECK(matched(prog, "@gmail.com") == false);
}

TEST_CASE("quoted string")
{
	auto prog = compile("\"([^\\\"]|\\.)*\"");
	CHECK(matched(prog, "\"\"") == true);
	CHECK(matched(prog, "\"my name is \\\"mostafa\\\"\"") == true);
	CHECK(matched(prog, "moustapha.saad.abdelhamed@gmail") == false);
	CHECK(matched(prog, "") == false);
}

TEST_CASE("arabic")
{
	auto prog = compile("أبجد+");
	CHECK(matched(prog, "أبجد") == true);
	CHECK(matched(prog, "أد") == false);
	CHECK(matched(prog, "أبجددددددد") == true);
	CHECK(matched(prog, "أبجذدددد") == false);
}

TEST_CASE("arabic set")
{
	auto prog = compile("[ء-ي]+");
	CHECK(matched(prog, "أبجد") == true);
	CHECK(matched(prog, "مصطفى") == true);
	CHECK(matched(prog, "mostafa") == false);
	CHECK(matched(prog, "") == false);
}

TEST_CASE("str runes iterator")
{
	mn::Rune runes[] = {'M', 'o', 's', 't', 'a', 'f', 'a'};
	size_t index = 0;
	for (auto c: mn::str_runes("Mostafa"))
	{
		CHECK(c == runes[index]);
		index++;
	}
	CHECK(index == 7);
}

TEST_CASE("executable path")
{
	mn::log_info("{}", mn::path_executable(mn::memory::tmp()));
}

TEST_CASE("arena scopes")
{
	mn::allocator_arena_free_all(mn::memory::tmp());

	auto name = mn::str_tmpf("my name is {}", "mostafa");
	auto empty_checkpoint = mn::allocator_arena_checkpoint(mn::memory::tmp());
	mn::allocator_arena_restore(mn::memory::tmp(), empty_checkpoint);
	CHECK(name == "my name is mostafa");

	void* ptr = nullptr;
	for (size_t i = 0; i < 10; ++i)
	{
		auto checkpoint = mn::allocator_arena_checkpoint(mn::memory::tmp());
		auto name = mn::str_tmpf("my name is {}", 100 - i);
		if (ptr == nullptr)
			ptr = name.ptr;
		CHECK(ptr == name.ptr);
		mn::allocator_arena_restore(mn::memory::tmp(), checkpoint);
	}

	auto checkpoint = mn::allocator_arena_checkpoint(mn::memory::tmp());
	for (size_t i = 0; i < 500; ++i)
	{
		auto name = mn::str_tmpf("my name is {}", i);
	}
	mn::allocator_arena_restore(mn::memory::tmp(), checkpoint);
	CHECK(name == "my name is mostafa");
}

TEST_CASE("str push blobs")
{
	auto str1 = mn::str_tmp("hello ");
	mn::str_push(str1, "w\0rld"_mnstr);
	CHECK(str1 == "hello w\0rld"_mnstr);
}

TEST_CASE("fmt str with null byte")
{
	CHECK(mn::str_tmpf("{}", "\0"_mnstr).count == 1);
	CHECK(mn::str_tmpf("{}", "\0B"_mnstr).count == 2);
	CHECK(mn::str_tmpf("{}", "A\0B"_mnstr).count == 3);
}

TEST_CASE("str_cmp")
{
	CHECK("AF"_mnstr > "AEF"_mnstr);
	CHECK("ABC"_mnstr < "DEF"_mnstr);
	CHECK("AB\0C"_mnstr < "AB\0D"_mnstr);
	CHECK("AB\0C"_mnstr > "AB"_mnstr);
	CHECK("AB"_mnstr < "AB\0\0"_mnstr);
	CHECK(""_mnstr < "AB\0\0"_mnstr);
	CHECK("AB\0\0"_mnstr > ""_mnstr);
	CHECK(""_mnstr == ""_mnstr);
}

TEST_CASE("config folder")
{
	auto config = mn::folder_config(mn::memory::tmp());
	mn::log_info("{}/file.txt", config);
}

TEST_CASE("json unpack")
{
	auto root = mn::json::value_object_new();
	mn::json::value_object_insert(root, "package", mn::json::value_string_new("sabre"));

	auto uniform = mn::json::value_object_new();
	mn::json::value_object_insert(uniform, "name", mn::json::value_string_new("camera"));
	mn::json::value_object_insert(uniform, "size", mn::json::value_number_new(16));

	mn::json::value_object_insert(root, "uniform", uniform);
	mn::json::value_object_insert(root, "leaf", mn::json::Value{});

	const char* package_name = "";
	const char* uniform_name = "";
	size_t uniform_size = 0;
	int sink = 0;

	auto err = mn::json::unpack(root, {
		{&package_name, "package"},
		{&uniform_name, "uniform.name"},
		{&uniform_size, "uniform.size"}
	});
	CHECK(err == false);
	CHECK(package_name == "sabre"_mnstr);
	CHECK(uniform_name == "camera"_mnstr);
	CHECK(uniform_size == 16);

	auto should_err = mn::json::unpack(root, {
		{&package_name, "name"},
		{&uniform_name, "uniform.name"},
		{&uniform_size, "uniform.size"}
	});
	CHECK(should_err == true);

	should_err = mn::json::unpack(root, {{&sink, "leaf.non_existing_key.another_non_existing_key"}});
	CHECK(should_err == true);

	mn::json::value_free(root);
}

TEST_CASE("map")
{
	auto set = mn::set_new<mn::Str>();

	mn::set_reserve(set, 6);
	mn::set_insert(set, "source"_mnstr);
	mn::set_insert(set, "jwt"_mnstr);
	mn::set_insert(set, "access"_mnstr);
	mn::set_insert(set, "refresh"_mnstr);
	mn::set_free(set);
}

TEST_CASE("str_join")
{
	auto numbers = {"5"_mnstr, "6"_mnstr, "7"_mnstr};
	auto result = mn::str_join(mn::memory::tmp(), begin(numbers), end(numbers), "|"_mnstr);
	CHECK(result == "5|6|7");
	result = mn::str_join(mn::memory::tmp(), begin(numbers), end(numbers), ","_mnstr);
	CHECK(result == "5,6,7");
	result = mn::str_join(mn::memory::tmp(), begin(numbers), end(numbers), " or "_mnstr);
	CHECK(result == "5 or 6 or 7");
}

TEST_CASE("folder_make_recursive")
{
	CHECK(mn::folder_make_recursive("a/b/c"));
	CHECK(mn::path_is_folder("a/b/c"));

	CHECK(mn::folder_make_recursive("a/b//"));
	CHECK(mn::path_is_folder("a/b//"));

	CHECK(mn::folder_make_recursive("a/b/"));
	CHECK(mn::path_is_folder("a/b/"));

	CHECK(mn::folder_make_recursive("a/"));
	CHECK(mn::path_is_folder("a/"));

	CHECK(mn::folder_make_recursive("a"));
	CHECK(mn::path_is_folder("a"));

	CHECK(mn::folder_make_recursive(""));
	CHECK(mn::folder_make_recursive("\\"));
}
