#include "mn/Thread.h"
#include "mn/Memory.h"
#include "mn/Fabric.h"
#include "mn/Map.h"
#include "mn/Defer.h"
#include "mn/Debug.h"
#include "mn/Log.h"
#include "mn/Assert.h"
#include "mn/winos/internal/Mutex.h"
#include "mn/winos/internal/Mutex_RW.h"
#include "mn/winos/internal/Thread.h"
#include "mn/winos/internal/Waitgroup.h"
#include "mn/winos/internal/Cond_Var.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <chrono>

namespace mn
{
	struct Leak_Allocator_Mutex
	{
		Source_Location srcloc;
		IMutex self;

		Leak_Allocator_Mutex()
		{
			srcloc.name = "allocators mutex";
			srcloc.function = "mn::_leak_allocator_mutex";
			srcloc.file = __FILE__;
			srcloc.line = __LINE__;
			srcloc.color = 0;
			self.name = srcloc.name;
			self.srcloc = &srcloc;
			InitializeCriticalSectionAndSpinCount(&self.cs, 1<<14);
			self.profile_user_data = _mutex_new(&self, self.name);
		}

		~Leak_Allocator_Mutex()
		{
			DeleteCriticalSection(&self.cs);
			_mutex_free(&self, self.profile_user_data);
		}
	};

	Mutex
	_leak_allocator_mutex()
	{
		static Leak_Allocator_Mutex mtx;
		return &mtx.self;
	}

	// Deadlock detector
	struct Mutex_Thread_Owner
	{
		DWORD id;
		int callstack_count;
		void* callstack[20];
	};

	struct Mutex_Deadlock_Reason
	{
		void* mtx;
		Mutex_Thread_Owner* owner;
	};

	struct Mutex_Ownership
	{
		enum KIND
		{
			KIND_EXCLUSIVE,
			KIND_SHARED,
		};

		KIND kind;
		union
		{
			Mutex_Thread_Owner exclusive;
			Map<DWORD, Mutex_Thread_Owner> shared;
		};
	};

	inline static Mutex_Ownership
	mutex_ownership_exclusive(DWORD thread_id)
	{
		Mutex_Ownership self{};
		self.kind = Mutex_Ownership::KIND_EXCLUSIVE;
		self.exclusive.id = thread_id;
		self.exclusive.callstack_count = (int)callstack_capture(self.exclusive.callstack, 20);
		return self;
	}

	inline static Mutex_Ownership
	mutex_ownership_shared()
	{
		Mutex_Ownership self{};
		self.kind = Mutex_Ownership::KIND_SHARED;
		self.shared = map_with_allocator<DWORD, Mutex_Thread_Owner>(memory::clib());
		return self;
	}

	inline static void
	mutex_ownership_free(Mutex_Ownership& self)
	{
		switch(self.kind)
		{
		case Mutex_Ownership::KIND_EXCLUSIVE:
			// do nothing
			break;
		case Mutex_Ownership::KIND_SHARED:
			map_free(self.shared);
			break;
		default:
			mn_unreachable();
			break;
		}
	}

	inline static void
	destruct(Mutex_Ownership& self)
	{
		mutex_ownership_free(self);
	}

	inline static void
	mutex_ownership_shared_add_owner(Mutex_Ownership& self, DWORD thread_id)
	{
		Mutex_Thread_Owner owner{};
		owner.id = thread_id;
		owner.callstack_count = (int)callstack_capture(owner.callstack, 20);
		map_insert(self.shared, thread_id, owner);
	}

	inline static void
	mutex_ownership_shared_remove_owner(Mutex_Ownership& self, DWORD thread_id)
	{
		map_remove(self.shared, thread_id);
	}

	inline static bool
	mutex_ownership_check(Mutex_Ownership& self, DWORD thread_id)
	{
		switch(self.kind)
		{
		case Mutex_Ownership::KIND_EXCLUSIVE:
			return self.exclusive.id == thread_id;
		case Mutex_Ownership::KIND_SHARED:
			return map_lookup(self.shared, thread_id) != nullptr;
		default:
			mn_unreachable();
			return false;
		}
	}

	inline static Mutex_Thread_Owner*
	mutex_ownership_get_owner(Mutex_Ownership& self, DWORD thread_id)
	{
		switch(self.kind)
		{
		case Mutex_Ownership::KIND_EXCLUSIVE:
			return &self.exclusive;
		case Mutex_Ownership::KIND_SHARED:
			return &map_lookup(self.shared, thread_id)->value;
		default:
			mn_unreachable();
			return nullptr;
		}
	}

	struct Deadlock_Detector
	{
		IMutex mtx;
		Map<void*, Mutex_Ownership> mutex_thread_owner;
		Map<DWORD, void*> thread_mutex_block;

		Deadlock_Detector()
		{
			mtx.name = "deadlock mutex";
			InitializeCriticalSectionAndSpinCount(&mtx.cs, 1<<14);
			mutex_thread_owner = map_with_allocator<void*, Mutex_Ownership>(memory::clib());
			thread_mutex_block = map_with_allocator<DWORD, void*>(memory::clib());
		}

		~Deadlock_Detector()
		{
			DeleteCriticalSection(&mtx.cs);
			destruct(mutex_thread_owner);
			map_free(thread_mutex_block);
		}
	};

	inline static Deadlock_Detector*
	_deadlock_detector()
	{
		static Deadlock_Detector _detector;
		return &_detector;
	}

	inline static bool
	_deadlock_detector_has_block_loop(Deadlock_Detector* self, void* mtx, DWORD thread_id, Buf<Mutex_Deadlock_Reason>& reasons)
	{
		auto it = map_lookup(self->mutex_thread_owner, mtx);
		if (it == nullptr)
			return false;

		bool deadlock_detected = false;
		DWORD reason_thread_id = thread_id;
		if (mutex_ownership_check(it->value, thread_id))
		{
			deadlock_detected = true;
		}
		else
		{
			switch(it->value.kind)
			{
			case Mutex_Ownership::KIND_EXCLUSIVE:
				if (auto block_it = map_lookup(self->thread_mutex_block, it->value.exclusive.id))
				{
					deadlock_detected = _deadlock_detector_has_block_loop(self, block_it->value, thread_id, reasons);
					reason_thread_id = it->value.exclusive.id;
				}
				break;
			case Mutex_Ownership::KIND_SHARED:
				for (auto [id, owner]: it->value.shared)
				{
					if (auto block_it = map_lookup(self->thread_mutex_block, id))
					{
						if (_deadlock_detector_has_block_loop(self, block_it->value, thread_id, reasons))
						{
							deadlock_detected = true;
							reason_thread_id = block_it->key;
							break;
						}
					}
				}
				break;
			default:
				mn_unreachable();
				break;
			}
		}

		if (deadlock_detected)
		{
			auto owner = mutex_ownership_get_owner(it->value, reason_thread_id);
			Mutex_Deadlock_Reason reason{};
			reason.mtx = it->key;
			reason.owner = owner;
			buf_push(reasons, reason);
			return true;
		}
		return false;
	}

	inline static void
	_deadlock_detector_mutex_block([[maybe_unused]] void* mtx)
	{
		#if MN_DEADLOCK
		auto self = _deadlock_detector();
		auto thread_id = GetCurrentThreadId();

		EnterCriticalSection(&self->mtx.cs);
		mn_defer{LeaveCriticalSection(&self->mtx.cs);};

		map_insert(self->thread_mutex_block, thread_id, mtx);

		Buf<Mutex_Deadlock_Reason> reasons{};
		if (_deadlock_detector_has_block_loop(self, mtx, thread_id, reasons))
		{
			log_error("Deadlock on mutex {} by thread #{} because of #{} reasons are listed below:", mtx, thread_id, reasons.count);
			void* callstack[20];
			auto callstack_count = callstack_capture(callstack, 20);
			callstack_print_to(callstack, callstack_count, file_stderr());
			printerr("\n");

			for (size_t i = 0; i < reasons.count; ++i)
			{
				auto ix = reasons.count - i - 1;
				auto reason = reasons[ix];

				auto block_it = map_lookup(self->thread_mutex_block, reason.owner->id);
				log_error(
					"reason #{}: Mutex {} was locked at the callstack listed below by thread #{} (while it was waiting for mutex {} to be released):",
					ix + 1,
					reason.mtx,
					reason.owner->id,
					block_it->value
				);
				callstack_print_to(reason.owner->callstack, reason.owner->callstack_count, file_stderr());
				printerr("\n");
			}
			::exit(-1);
		}
		buf_free(reasons);

		#endif
	}

	inline static void
	_deadlock_detector_mutex_set_exclusive_owner([[maybe_unused]] void* mtx)
	{
		#if MN_DEADLOCK
		auto self = _deadlock_detector();
		auto thread_id = GetCurrentThreadId();

		EnterCriticalSection(&self->mtx.cs);
		mn_defer{LeaveCriticalSection(&self->mtx.cs);};

		if (auto it = map_lookup(self->mutex_thread_owner, mtx))
		{
			panic("Deadlock on mutex {} by thread #{}", mtx, thread_id);
		}

		map_remove(self->thread_mutex_block, thread_id);
		map_insert(self->mutex_thread_owner, mtx, mutex_ownership_exclusive(thread_id));
		#endif
	}

	inline static void
	_deadlock_detector_mutex_set_shared_owner([[maybe_unused]] void* mtx)
	{
		#if MN_DEADLOCK
		auto self = _deadlock_detector();
		auto thread_id = GetCurrentThreadId();

		EnterCriticalSection(&self->mtx.cs);
		mn_defer{LeaveCriticalSection(&self->mtx.cs);};

		map_remove(self->thread_mutex_block, thread_id);
		if (auto it = map_lookup(self->mutex_thread_owner, mtx))
		{
			mutex_ownership_shared_add_owner(it->value, thread_id);
		}
		else
		{
			auto mutex_ownership = mutex_ownership_shared();
			mutex_ownership_shared_add_owner(mutex_ownership, thread_id);
			map_insert(self->mutex_thread_owner, mtx, mutex_ownership);
		}
		#endif
	}

	inline static void
	_deadlock_detector_mutex_unset_owner([[maybe_unused]] void* mtx)
	{
		#if MN_DEADLOCK
		auto self = _deadlock_detector();
		auto thread_id = GetCurrentThreadId();

		EnterCriticalSection(&self->mtx.cs);
		mn_defer{LeaveCriticalSection(&self->mtx.cs);};

		auto it = map_lookup(self->mutex_thread_owner, mtx);
		switch(it->value.kind)
		{
		case Mutex_Ownership::KIND_EXCLUSIVE:
			map_remove(self->mutex_thread_owner, mtx);
			break;
		case Mutex_Ownership::KIND_SHARED:
			mutex_ownership_shared_remove_owner(it->value, thread_id);
			if (it->value.shared.count == 0)
			{
				mutex_ownership_free(it->value);
				map_remove(self->mutex_thread_owner, mtx);
			}
			break;
		default:
			mn_unreachable();
			break;
		}
		#endif
	}

	// API
	Mutex
	mutex_new_with_srcloc(const Source_Location* srcloc)
	{
		auto self = alloc<IMutex>();
		self->srcloc = srcloc;
		self->name = srcloc->name;
		InitializeCriticalSectionAndSpinCount(&self->cs, 1<<14);
		self->profile_user_data = _mutex_new(self, self->name);

		return self;
	}

	Mutex
	mutex_new(const char* name)
	{
		auto self = alloc<IMutex>();
		self->srcloc = nullptr;
		self->name = name;
		InitializeCriticalSectionAndSpinCount(&self->cs, 1<<14);
		self->profile_user_data = _mutex_new(self, self->name);

		return self;
	}

	void
	mutex_lock(Mutex self)
	{
		auto call_after_lock = _mutex_before_lock(self, self->profile_user_data);
		mn_defer{
			if (call_after_lock)
				_mutex_after_lock(self, self->profile_user_data);
		};

		if (TryEnterCriticalSection(&self->cs))
		{
			_deadlock_detector_mutex_set_exclusive_owner(self);
			return;
		}

		worker_block_ahead();
		_deadlock_detector_mutex_block(self);
		EnterCriticalSection(&self->cs);
		_deadlock_detector_mutex_set_exclusive_owner(self);
		worker_block_clear();
	}

	void
	mutex_unlock(Mutex self)
	{
		_deadlock_detector_mutex_unset_owner(self);
		LeaveCriticalSection(&self->cs);
		_mutex_after_unlock(self, self->profile_user_data);
	}

	void
	mutex_free(Mutex self)
	{
		_mutex_free(self, self->profile_user_data);
		DeleteCriticalSection(&self->cs);
		free(self);
	}

	const Source_Location*
	mutex_source_location(Mutex self)
	{
		return self->srcloc;
	}


	//Mutex_RW API
	Mutex_RW
	mutex_rw_new_with_srcloc(const Source_Location* srcloc)
	{
		Mutex_RW self = alloc<IMutex_RW>();
		self->lock = SRWLOCK_INIT;
		self->name = srcloc->name;
		self->srcloc = srcloc;
		self->profile_user_data = _mutex_rw_new(self, self->name);
		return self;
	}

	Mutex_RW
	mutex_rw_new(const char* name)
	{
		Mutex_RW self = alloc<IMutex_RW>();
		self->lock = SRWLOCK_INIT;
		self->name = name;
		self->srcloc = nullptr;
		self->profile_user_data = _mutex_rw_new(self, self->name);
		return self;
	}

	void
	mutex_rw_free(Mutex_RW self)
	{
		_mutex_rw_free(self, self->profile_user_data);
		free(self);
	}

	void
	mutex_read_lock(Mutex_RW self)
	{
		auto call_after_lock = _mutex_before_read_lock(self, self->profile_user_data);
		mn_defer{
			if (call_after_lock)
				_mutex_after_read_lock(self, self->profile_user_data);
		};

		if (TryAcquireSRWLockShared(&self->lock))
		{
			_deadlock_detector_mutex_set_shared_owner(self);
			return;
		}

		worker_block_ahead();
		_deadlock_detector_mutex_block(self);
		AcquireSRWLockShared(&self->lock);
		_deadlock_detector_mutex_set_shared_owner(self);
		worker_block_clear();
	}

	void
	mutex_read_unlock(Mutex_RW self)
	{
		_deadlock_detector_mutex_unset_owner(self);
		ReleaseSRWLockShared(&self->lock);
		_mutex_after_read_unlock(self, self->profile_user_data);
	}

	void
	mutex_write_lock(Mutex_RW self)
	{
		auto call_after_lock = _mutex_before_write_lock(self, self->profile_user_data);
		mn_defer{
			if (call_after_lock)
				_mutex_after_write_lock(self, self->profile_user_data);
		};

		if (TryAcquireSRWLockExclusive(&self->lock))
		{
			_deadlock_detector_mutex_set_exclusive_owner(self);
			return;
		}

		worker_block_ahead();
		_deadlock_detector_mutex_block(self);
		AcquireSRWLockExclusive(&self->lock);
		_deadlock_detector_mutex_set_exclusive_owner(self);
		worker_block_clear();
	}

	void
	mutex_write_unlock(Mutex_RW self)
	{
		_deadlock_detector_mutex_unset_owner(self);
		ReleaseSRWLockExclusive(&self->lock);
		_mutex_after_write_unlock(self, self->profile_user_data);
	}

	const Source_Location*
	mutex_rw_source_location(Mutex_RW self)
	{
		return self->srcloc;
	}


	//Thread API
	DWORD WINAPI
	_thread_start(LPVOID user_data)
	{
		Thread self = (Thread)user_data;

		_thread_new(self, self->name);

		if(self->func)
			self->func(self->user_data);
		return 0;
	}

	Thread
	thread_new(Thread_Func func, void* arg, const char* name)
	{
		Thread self = alloc<IThread>();
		self->func = func;
		self->user_data = arg;
		self->name = name;

		self->handle = CreateThread(NULL, //default security attributes
									0, //default stack size
									_thread_start, //thread start function
									self, //thread start function arg
									0, //default creation flags
									&self->id); //thread id

#ifndef NDEBUG
		// Setting a thread name is useful when debugging
		{
			/*
			 * SetThreadDescription may not be available on this version of Windows,
			 * and testing the operating system version is not the best way to do this.
			 * First, it requires the application/library to be manifested for Windows 10,
			 * and even then, the functionality may be available through a redistributable DLL.
			 *
			 * Instead, check that the function exists in the appropriate library (kernel32.dll).
			 *
			 * Reference: (https://docs.microsoft.com/en-us/windows/win32/sysinfo/operating-system-version)
			 */
			HMODULE kernel = LoadLibrary(L"kernel32.dll");
			if (kernel != nullptr)
			{
				mn_defer{FreeLibrary(kernel);};

				using prototype = HRESULT(*)(HANDLE, PCWSTR);
				auto set_thread_description = (prototype)GetProcAddress(kernel, "SetThreadDescription");
				if (set_thread_description != nullptr)
				{
					int buffer_size = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
					Block buffer = alloc(buffer_size * sizeof(WCHAR), alignof(WCHAR));
					mn_defer{free(buffer);};
					MultiByteToWideChar(CP_UTF8, 0, name, -1, (LPWSTR)buffer.ptr, buffer_size);

					set_thread_description(self->handle, (LPWSTR)buffer.ptr);
				}
			}
		}
#endif

		return self;
	}

	void
	thread_free(Thread self)
	{
		if(self->handle)
		{
			[[maybe_unused]] BOOL result = CloseHandle(self->handle);
			mn_assert(result == TRUE);
		}
		free(self);
	}

	void
	thread_join(Thread self)
	{
		worker_block_ahead();
		if(self->handle)
		{
			[[maybe_unused]] DWORD result = WaitForSingleObject(self->handle, INFINITE);
			mn_assert(result == WAIT_OBJECT_0);
		}
		worker_block_clear();
	}

	void
	thread_sleep(uint32_t milliseconds)
	{
		Sleep(DWORD(milliseconds));
	}

	void*
	thread_id()
	{
		return (void*)(uintptr_t)GetCurrentThreadId();
	}


	// time
	uint64_t
	time_in_millis()
	{
		auto tp = std::chrono::high_resolution_clock::now().time_since_epoch();
		return std::chrono::duration_cast<std::chrono::milliseconds>(tp).count();
	}


	// Condition Variable
	Cond_Var
	cond_var_new()
	{
		auto self = alloc<ICond_Var>();
		InitializeConditionVariable(&self->cv);
		return self;
	}

	void
	cond_var_free(Cond_Var self)
	{
		free(self);
	}

	void
	cond_var_wait(Cond_Var self, Mutex mtx)
	{
		_mutex_after_unlock(mtx, mtx->profile_user_data);
		mn_defer{_mutex_after_lock(mtx, mtx->profile_user_data);};

		worker_block_ahead();
		_deadlock_detector_mutex_unset_owner(mtx);
		SleepConditionVariableCS(&self->cv, &mtx->cs, INFINITE);
		_deadlock_detector_mutex_set_exclusive_owner(mtx);
		worker_block_clear();
	}

	Cond_Var_Wake_State
	cond_var_wait_timeout(Cond_Var self, Mutex mtx, uint32_t millis)
	{
		_mutex_after_unlock(mtx, mtx->profile_user_data);
		mn_defer{_mutex_after_lock(mtx, mtx->profile_user_data);};

		worker_block_ahead();
		_deadlock_detector_mutex_unset_owner(mtx);
		auto res = SleepConditionVariableCS(&self->cv, &mtx->cs, millis);
		_deadlock_detector_mutex_set_exclusive_owner(mtx);
		worker_block_clear();

		if (res)
			return Cond_Var_Wake_State::SIGNALED;

		if (GetLastError() == ERROR_TIMEOUT)
			return Cond_Var_Wake_State::TIMEOUT;

		return Cond_Var_Wake_State::SPURIOUS;
	}

	void
	cond_var_notify(Cond_Var self)
	{
		WakeConditionVariable(&self->cv);
	}

	void
	cond_var_notify_all(Cond_Var self)
	{
		WakeAllConditionVariable(&self->cv);
	}

	// Waitgroup
	Waitgroup
	waitgroup_new()
	{
		auto self = alloc<IWaitgroup>();
		self->count = 0;
		InitializeCriticalSectionAndSpinCount(&self->cs, 1<<14);
		InitializeConditionVariable(&self->cv);
		return self;
	}

	void
	waitgroup_free(Waitgroup self)
	{
		DeleteCriticalSection(&self->cs);
		free(self);
	}

	void
	waitgroup_wait(Waitgroup self)
	{
		worker_block_ahead();
		mn_defer{worker_block_clear();};

		EnterCriticalSection(&self->cs);
		mn_defer{LeaveCriticalSection(&self->cs);};

		while(self->count > 0)
			SleepConditionVariableCS(&self->cv, &self->cs, INFINITE);

		mn_assert(self->count == 0);
	}

	void
	waitgroup_add(Waitgroup self, int c)
	{
		mn_assert(c > 0);

		EnterCriticalSection(&self->cs);
		mn_defer{LeaveCriticalSection(&self->cs);};

		self->count += c;
	}

	void
	waitgroup_done(Waitgroup self)
	{
		EnterCriticalSection(&self->cs);
		mn_defer{LeaveCriticalSection(&self->cs);};

		--self->count;
		mn_assert(self->count >= 0);

		if (self->count == 0)
			WakeAllConditionVariable(&self->cv);
	}

	int
	waitgroup_count(Waitgroup self)
	{
		EnterCriticalSection(&self->cs);
		mn_defer{LeaveCriticalSection(&self->cs);};
		return self->count;
	}
}
