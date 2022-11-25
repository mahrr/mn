#include "mn/Assert.h"
#include "mn/Log.h"

namespace mn
{
	void
	_report_assert_message(const char* expr, const char* message, const char* file, int line)
	{
		Str msg{};
		if (message)
			msg = str_tmpf("Assertion Failure: {}, message: {}, in file: {}, line: {}", expr, message, file, line);
		else
			msg = str_tmpf("Assertion Failure: {}, in file: {}, line: {}", expr, file, line);
		_log_critical_str(msg.ptr);
	}
}