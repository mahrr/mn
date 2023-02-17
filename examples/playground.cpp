#include <mn/IO.h>
#include <mn/Msgpack.h>
#include <mn/Log.h>

struct Person
{
	mn::Str name;
	int age;
};

template<typename TArchive>
inline static mn::Err
msgpack(TArchive& self, const Person& p)
{
	return mn::msgpack_struct(self, {
		{"name", &p.name},
		{"age", &p.age},
	});
}

inline static void
test_person(const Person& p)
{
	auto [bytes, encode_err] = mn::msgpack_encode(p);
	if (encode_err)
		mn::log_error("encode error, {}", encode_err);

	mn::print("[");
	for (size_t i = 0; i < bytes.count; ++i)
	{
		if (i > 0)
			mn::print(", ");
		mn::print("{:d}", bytes[i]);
	}
	mn::print("]\n");

	Person out{};
	auto decode_err = mn::msgpack_decode(bytes, out);
	if (decode_err)
		mn::log_error("decode error, {}", decode_err);

	if (out.name != p.name)
		mn::log_error("name mismatch {} != {}", out.name, p.name);

	if (out.age != p.age)
		mn::log_error("age mismatch {} != {}", out.age, p.age);
}

int main(int argc, char** argv)
{
	Person mostafa{"Mostafa"_mnstr, 29};
	Person abdelfattah{"abdelfattah"_mnstr, 30};
	Person abdelhameed{"abdelhameed"_mnstr, 27};

	test_person(mostafa);
	test_person(abdelfattah);
	test_person(abdelhameed);

	mn::print("Hello, World!\n");
	return 0;
}
