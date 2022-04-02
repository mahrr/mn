#pragma once

#include "mn/Exports.h"
#include "mn/Base.h"
#include "mn/OS.h"

#include <stdint.h>

#define mn_mutex_new_with_srcloc(name) mn::mutex_new_with_srcloc([&](const char* func_name) -> const mn::Source_Location* { const static mn::Source_Location srcloc { name, func_name, __FILE__, __LINE__, 0 }; return &srcloc; }(__FUNCTION__))
#define mn_mutex_rw_new_with_srcloc(name) mn::mutex_rw_new_with_srcloc([&](const char* func_name) -> const mn::Source_Location* { const static mn::Source_Location srcloc { name, func_name, __FILE__, __LINE__, 0 }; return &srcloc; }(__FUNCTION__))

namespace mn
{
	// mutex handle
	typedef struct IMutex* Mutex;

	MN_EXPORT Mutex
	_leak_allocator_mutex();

	// creates a new mutex with the given source location info, which is useful for debugging and profiling
	MN_EXPORT Mutex
	mutex_new_with_srcloc(const Source_Location* srcloc);

	// creates a new mutex
	MN_EXPORT Mutex
	mutex_new(const char* name = "Mutex");

	// locks the mutex, it will block until the lock is acquired
	MN_EXPORT void
	mutex_lock(Mutex mutex);

	// unlocks the mutex
	MN_EXPORT void
	mutex_unlock(Mutex mutex);


	// frees the mutex
	MN_EXPORT void
	mutex_free(Mutex mutex);

	// returns the source location if the mutex was created with srcloc, nullptr otherwise
	MN_EXPORT const Source_Location*
	mutex_source_location(Mutex mutex);

	// destruct overload for mutex free
	inline static void
	destruct(Mutex mutex)
	{
		mutex_free(mutex);
	}


	// read preferring multi-reader single-writer mutex
	typedef struct IMutex_RW* Mutex_RW;

	// creates a new read-write mutex with the given source location info, which is useful for debugging and profiling
	MN_EXPORT Mutex_RW
	mutex_rw_new_with_srcloc(const Source_Location* srcloc);

	// creates a new mutex with the given name
	MN_EXPORT Mutex_RW
	mutex_rw_new(const char* name = "Mutex_RW");

	// frees the mutex
	MN_EXPORT void
	mutex_rw_free(Mutex_RW mutex);

	// destruct overload for read-write mutex free
	inline static void
	destruct(Mutex_RW mutex)
	{
		mutex_rw_free(mutex);
	}

	// locks the mutex for read operation, it will block until a lock is acquired
	MN_EXPORT void
	mutex_read_lock(Mutex_RW mutex);

	// unlocks the mutex from a read lock
	MN_EXPORT void
	mutex_read_unlock(Mutex_RW mutex);

	// locks the mutex for write operation, it will block until a lock is acquired
	MN_EXPORT void
	mutex_write_lock(Mutex_RW mutex);

	// unlocks the mutex from a write operation
	MN_EXPORT void
	mutex_write_unlock(Mutex_RW mutex);

	// returns the source location if the mutex was created with srcloc, nullptr otherwise
	MN_EXPORT const Source_Location*
	mutex_rw_source_location(Mutex_RW mutex);


	//Thread API

	// a thread handle
	typedef struct IThread* Thread;

	// thread function
	using Thread_Func = void(*)(void*);

	// creates a new thread with the given function, argument, and name
	MN_EXPORT Thread
	thread_new(Thread_Func func, void* arg, const char* name = "Thread");

	// frees the given thread handle
	MN_EXPORT void
	thread_free(Thread thread);

	// destruct overload for thread free
	inline static void
	destruct(Thread thread)
	{
		thread_free(thread);
	}

	// joins the execution of the calling thread with the given thread
	MN_EXPORT void
	thread_join(Thread thread);

	// sleeps the calling thread by the given milliseconds
	MN_EXPORT void
	thread_sleep(uint32_t milliseconds);

	// returns the id of the calling thread
	MN_EXPORT void*
	thread_id();


	// returns time in milliseonds
	MN_EXPORT uint64_t
	time_in_millis();


	// a condition variable handle
	typedef struct ICond_Var* Cond_Var;

	// condition variable wake state
	enum class Cond_Var_Wake_State
	{
		// condition variable waked because it was signalled
		SIGNALED,
		// condition variable waked because it timed out
		TIMEOUT,
		// a spurious wake from OS scheduler
		SPURIOUS
	};

	// creats a new condition variable
	MN_EXPORT Cond_Var
	cond_var_new();

	// frees the given condition variable
	MN_EXPORT void
	cond_var_free(Cond_Var self);

	// destruct overload for condition variable free
	inline static void
	destruct(Cond_Var self)
	{
		cond_var_free(self);
	}

	// waits on the condition variable
	MN_EXPORT void
	cond_var_wait(Cond_Var self, Mutex mtx);

	// waits on the condition variable until the function returns true
	template<typename TFunc>
	inline static void
	cond_var_wait(Cond_Var self, Mutex mtx, TFunc&& func)
	{
		while (func() == false)
			cond_var_wait(self, mtx);
	}

	// waits on the condition variable with a timeout
	MN_EXPORT Cond_Var_Wake_State
	cond_var_wait_timeout(Cond_Var self, Mutex mtx, uint32_t millis);

	// waits on the condition variable until the function returns true or until it times out
	template<typename TFunc>
	inline static bool
	cond_var_wait_timeout(Cond_Var self, Mutex mtx, uint32_t millis, TFunc&& func)
	{
		auto start_time = time_in_millis();
		while (func() == false)
		{
			auto state = cond_var_wait_timeout(self, mtx, millis);
			if (state == Cond_Var_Wake_State::SIGNALED)
			{
				return true;
			}
			else if (state == Cond_Var_Wake_State::TIMEOUT)
			{
				return false;
			}
			else
			{
				auto end_time = time_in_millis();
				if (end_time - start_time >= millis)
					return false;
			}
		}
		return true;
	}

	// signals a condition variable waiter
	MN_EXPORT void
	cond_var_notify(Cond_Var self);

	// signals all condition variable waiters
	MN_EXPORT void
	cond_var_notify_all(Cond_Var self);

	// a waitgroup is a sync primitive which is a counter you can wait on until it reaches zero
	typedef struct IWaitgroup* Waitgroup;

	// creates a new waitgroup
	MN_EXPORT Waitgroup
	waitgroup_new();

	// frees the given waitgroup
	MN_EXPORT void
	waitgroup_free(Waitgroup self);

	// waits until the waitgroup is zero
	MN_EXPORT void
	waitgroup_wait(Waitgroup self);

	// adds c to the waitgroup counter
	MN_EXPORT void
	waitgroup_add(Waitgroup self, int c);

	// decrements the waitgroup counter and signals it if it reaches 0
	MN_EXPORT void
	waitgroup_done(Waitgroup self);

	// returns the waitgroup count, >0 means it's not done yet, 0 done, <0 shouldn't happen
	MN_EXPORT int
	waitgroup_count(Waitgroup self);

	// automatic waitgroup which uses RAII to manage its memory
	// useful in case you want a quick scoped waitgroup
	struct Auto_Waitgroup
	{
		Waitgroup handle;

		Auto_Waitgroup()
			:handle(waitgroup_new())
		{}

		~Auto_Waitgroup()
		{
			if (this->handle)
				waitgroup_free(this->handle);
		}

		Auto_Waitgroup(const Auto_Waitgroup&) = delete;

		Auto_Waitgroup(Auto_Waitgroup&& other)
		{
			this->handle = other.handle;
			other.handle = nullptr;
		}

		Auto_Waitgroup&
		operator=(const Auto_Waitgroup& other) = delete;

		Auto_Waitgroup&
		operator=(Auto_Waitgroup&& other)
		{
			if (this->handle)
				waitgroup_free(this->handle);

			this->handle = other.handle;
			other.handle = nullptr;

			return *this;
		}

		// adds c to the waitgroup counter
		void
		add(int c)
		{
			waitgroup_add(handle, c);
		}

		// decrements the waitgroup counter and signals it if it reaches 0
		void
		done()
		{
			waitgroup_done(handle);
		}

		// waits until the waitgroup is zero
		void
		wait()
		{
			waitgroup_wait(handle);
		}
	};
}
