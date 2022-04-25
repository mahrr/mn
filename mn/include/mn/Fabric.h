#pragma once

#include "mn/Exports.h"
#include "mn/Base.h"
#include "mn/Task.h"
#include "mn/Ring.h"
#include "mn/Thread.h"
#include "mn/Defer.h"
#include "mn/OS.h"
#include "mn/Stream.h"
#include "mn/Assert.h"

#include <atomic>
#include <chrono>
#include <type_traits>

namespace mn
{
	// represents the compute interface dimensions which is used to specify
	// how many tasks you need along x, y, and z axis
	// similar to graphics compute dispatch interface
	struct Compute_Dims
	{
		size_t x, y, z;
	};

	// represents the parameter for each compute job, it contains the same
	// info as the graphics compute dispatch interface
	struct Compute_Args
	{
		// the size of local workgroup (potentially the size which a single thread will handle)
		// this is the local dimension input passed to the compute interface
		Compute_Dims workgroup_size;
		// the number of the local workgroups in this compute dispatch call
		// this is the global dimension input passed to the compute interface
		Compute_Dims workgroup_num;
		// current global id of the local workgroup (indexes of the workgroup_num)
		Compute_Dims workgroup_id;
		// current local id of within the local workgroup (indexed of the workgroup_size)
		Compute_Dims local_invocation_id;
		// global id of the current compute invokation (workgroup_id * workgroup_size + local_invocation_id)
		// index within both local + global indices of the compute dispatch
		Compute_Dims global_invocation_id;
		// tile size, this is usually equal to workgroup_size, but if the workgroup_num is not divisible
		// by workgroup_size this will contain the exact tile size which should fit within workgroup_num
		Compute_Dims tile_size;
	};

	// represents a single task in the fabric's worker task queue
	struct Fabric_Task
	{
		enum KIND
		{
			// a oneshot task, usually invoked via go function
			KIND_ONESHOT,
			// a compute task, usually invoked via compute function
			KIND_COMPUTE,
		};

		KIND kind;
		union
		{
			struct
			{
				Task<void()> task;
			} as_oneshot;

			struct
			{
				Task<void(Compute_Args)> task;
				Compute_Args args;
				Waitgroup wg;
			} as_compute;
		};
	};

	// runs the given fabric task instance
	inline static void
	fabric_task_run(Fabric_Task& self)
	{
		switch (self.kind)
		{
		case Fabric_Task::KIND_ONESHOT:
			self.as_oneshot.task();
			break;
		case Fabric_Task::KIND_COMPUTE:
			self.as_compute.task(self.as_compute.args);
			if (self.as_compute.wg) waitgroup_done(self.as_compute.wg);
			break;
		default:
			break;
		}
	}

	// frees the given fabric task
	inline static void
	fabric_task_free(Fabric_Task& self)
	{
		switch (self.kind)
		{
		case Fabric_Task::KIND_ONESHOT:
			task_free(self.as_oneshot.task);
			break;
		case Fabric_Task::KIND_COMPUTE:
			task_free(self.as_compute.task);
			break;
		default:
			break;
		}
	}

	// destruct overload for the fabric task free function
	inline static void
	destruct(Fabric_Task& self)
	{
		fabric_task_free(self);
	}

	// Worker
	// represents a fabric worker which is a thread with a job queue attached to it
	typedef struct IWorker* Worker;

	// creates a new worker which is a threads with a job queue
	MN_EXPORT Worker
	worker_new(const char* name);

	// frees the worker and stops its thread
	MN_EXPORT void
	worker_free(Worker self);

	// destruct overload for the worker
	inline static void
	destruct(Worker self)
	{
		worker_free(self);
	}

	// schedules a task into the worker queue
	MN_EXPORT void
	worker_task_do(Worker self, const Fabric_Task& task);

	// schedules a task batch in to worker queue
	MN_EXPORT void
	worker_task_batch_do(Worker self, const Fabric_Task* ptr, size_t count);

	// schedules any callable into the worker queue
	template<typename TFunc>
	inline static void
	worker_do(Worker self, TFunc&& f)
	{
		Fabric_Task entry{};
		entry.as_oneshot.task = Task<void()>::make(std::forward<TFunc>(f));
		worker_task_do(self, entry);
	}

	// returns the local worker of the calling thread if it has one, if it doesn't have a worker it will return nullptr
	MN_EXPORT Worker
	worker_local();

	// used to signal to fabric that the calling thread's worker will do something that will
	// potentially block worker's thread which is useful meta info for sysmon to decide what
	// will it do to worker's unscheduled jobs
	// blocking work examples: sleep, disk IO, network IO, mutex, etc..
	MN_EXPORT void
	worker_block_ahead();

	// used to signal to fabric that the calling thread's worker has returned from the blocking workload and is
	// executing actual code
	MN_EXPORT void
	worker_block_clear();

	// blocks the current thread execution until the given function returns true
	// it will check the function periodically (every 1 ms)
	template<typename TFunc>
	inline static void
	worker_block_on(TFunc&& fn)
	{
		worker_block_ahead();
		while(fn() == false)
			thread_sleep(1);
		worker_block_clear();
	}

	// blocks the current thread execution until the given function returns true, or until it times out
	// it will check the function periodically (every 1 ms)
	template<typename TFunc>
	inline static void
	worker_block_on_with_timeout(Timeout timeout, TFunc&& fn)
	{
		worker_block_ahead();
		auto start = std::chrono::steady_clock::now();
		while(fn() == false)
		{
			if (timeout == NO_TIMEOUT)
			{
				break;
			}
			else if (timeout != INFINITE_TIMEOUT)
			{
				auto t = std::chrono::steady_clock::now();
				if ((uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(t - start).count() >= timeout.milliseconds)
					break;
			}
			thread_sleep(1);
		}
		worker_block_clear();
	}

	// returns the current worker index within its fabric, returns 0 if it doesn't belong to a fabric, and -1 if this
	// function is called from non-worker thread
	MN_EXPORT int
	local_worker_index();


	// fabric is a job queue system with multiple workers which it uses to execute jobs effieciently
	typedef struct IFabric* Fabric;

	// fabric construction settings, which is used to customize fabric behavior on creation
	struct Fabric_Settings
	{
		// fabric instance name
		const char* name;
		// default: CPU cores count
		size_t workers_count;
		// default: 1/2 CPU cores count
		size_t put_aside_worker_count;
		// how many milliseconds sysmon will wait for before declaring this worker blocked
		// in case this worker announced that it will block via worker_block_ahead, worker_block_clear
		// default: 10
		uint32_t coop_blocking_threshold_in_ms;
		// how many milliseconds sysmon will wait for before declaring this worker blocked
		// in case this worker didn't announce that it will block via worker_block_ahead, worker_block_clear API
		// default: 1000
		uint32_t external_blocking_threshold_in_ms;
		// threshold of the blocking workers ratio [0, 1] which sysmon
		// will use to start evicting these workers if the blocking_workers_count >= workers_count * blocking_workers_threshold
		// default: 0.5f
		float blocking_workers_threshold;
		// function which will be executed after each worker finishes executing a job
		Task<void()> after_each_job;
		// function which will be executed when a new worker is started
		Task<void()> on_worker_start;
	};

	// creates a new fabric instance with the given construction settings
	MN_EXPORT Fabric
	fabric_new(Fabric_Settings settings);

	// stops and frees the given fabric
	MN_EXPORT void
	fabric_free(Fabric self);

	// destruct overload of fabric_free fabric
	inline static void
	destruct(Fabric self)
	{
		fabric_free(self);
	}

	// adds a task to the fabric
	MN_EXPORT void
	fabric_task_do(Fabric self, const Fabric_Task& task);

	// adds a batch of tasks to fabric
	MN_EXPORT void
	fabric_task_batch_do(Fabric self, const Fabric_Task* ptr, size_t count);

	// schedules any callable into the fabric queue
	template<typename TFunc>
	inline static void
	fabric_do(Fabric self, TFunc&& f)
	{
		Fabric_Task entry{};
		entry.as_oneshot.task = Task<void()>::make(std::forward<TFunc>(f));
		fabric_task_do(self, entry);
	}

	// returns the local fabric of the calling thread if it has one, if it doesn't it will return nullptr
	MN_EXPORT Fabric
	fabric_local();

	// returns the number of workers of the given fabric instance
	MN_EXPORT size_t
	fabric_workers_count(Fabric self);

	// schedules the given callable into the given fabric
	template<typename TFunc, typename ... TArgs>
	inline static void
	go(Fabric f, TFunc&& fn, TArgs&& ... args)
	{
		Fabric_Task entry{};
		entry.as_oneshot.task = Task<void()>::make([=]() mutable { fn(args...); });
		fabric_task_do(f, entry);
	}

	// schedules the given callable into the given worker
	template<typename TFunc, typename ... TArgs>
	inline static void
	go(Worker worker, TFunc&& fn, TArgs&& ... args)
	{
		Fabric_Task entry{};
		entry.as_oneshot.task = Task<void()>::make([=]() mutable { fn(args...); });
		worker_task_do(worker, entry);
	}

	// a message passing primitive used to communicate between fabric tasks
	// this one is built around messages being simple byte streams
	// which is useful if you're going to do work like encryption/compression
	typedef struct IChan_Stream* Chan_Stream;
	struct IChan_Stream final: IStream
	{
		Block data_blob;
		Mutex mtx;
		Cond_Var read_cv;
		Cond_Var write_cv;
		std::atomic<int32_t> atomic_arc;
		std::atomic<bool> atomic_closed;

		// disposes of the current state of the channel stream
		MN_EXPORT void
		dispose() override;

		// tries to read into the data_out block and returns the actual number of read bytes
		MN_EXPORT size_t
		read(Block data_out) override;

		// tried to write data_in block into the given stream and returns the actual number of written bytes
		MN_EXPORT size_t
		write(Block data_in) override;

		// returns 0 because we don't know the size of the stream beforehand
		int64_t
		size() override
		{
			return 0;
		};

		// it always fails because a stream doesn't have cursor support
		int64_t
		cursor_operation(STREAM_CURSOR_OP, int64_t) override
		{
			mn_unreachable_msg("Chan_Stream doesn't support cursor operations");
			return STREAM_CURSOR_ERROR;
		}
	};

	// creates a new channel stream which is a message passing primitive used
	// to communicate between fabric tasks in byte/binary messages
	MN_EXPORT Chan_Stream
	chan_stream_new();

	// frees the given channel stream, by decrementing its reference count and only freeing it if it reaches 0
	MN_EXPORT void
	chan_stream_free(Chan_Stream self);

	// increments the reference count of the given stream
	// because this stream is used to communicate between threads, it follows that
	// its ownership model is not unique to single thread, but it's shared between multiple threads
	// that's why it uses a reference counting model for its ownership
	MN_EXPORT Chan_Stream
	chan_stream_ref(Chan_Stream self);

	// decrements the reference count of the given stream and frees the stream if it reaches 0
	// because this stream is used to communicate between threads, it follows that
	// its ownership model is not unique to single thread, but it's shared between multiple threads
	// that's why it uses a reference counting model for its ownership
	MN_EXPORT void
	chan_stream_unref(Chan_Stream self);

	// closes the given channel stream which means that any subsequent writes will fail
	MN_EXPORT void
	chan_stream_close(Chan_Stream self);

	// returns whether the given channel stream is closed
	MN_EXPORT bool
	chan_stream_closed(Chan_Stream self);

	// automatic wrapper around channel stream which uses RAII to handle the reference counting
	// useful for scoped usage of channel streams
	struct Auto_Chan_Stream
	{
		Chan_Stream handle;

		Auto_Chan_Stream()
			: handle(chan_stream_new())
		{}

		explicit Auto_Chan_Stream(Chan_Stream s)
			: handle(chan_stream_ref(s))
		{}

		Auto_Chan_Stream(const Auto_Chan_Stream& other)
			: handle(chan_stream_ref(other.handle))
		{}

		Auto_Chan_Stream(Auto_Chan_Stream&& other)
			: handle(other.handle)
		{
			other.handle = nullptr;
		}

		Auto_Chan_Stream&
		operator=(const Auto_Chan_Stream& other)
		{
			if (handle) chan_stream_free(handle);
			handle = chan_stream_ref(other.handle);
			return *this;
		}

		Auto_Chan_Stream&
		operator=(Auto_Chan_Stream&& other)
		{
			if (handle) chan_stream_free(handle);
			handle = other.handle;
			other.handle = nullptr;
			return *this;
		}

		~Auto_Chan_Stream()
		{
			if (handle)
				chan_stream_free(handle);
		}

		operator Chan_Stream() const { return handle; }
	};

	// converts a given stream from an active one to a lazy one which is much suitable for piping a
	// single stream through different processing functions
	// for example if i have a file stream which is streamed from disk, ex. `auto file = file_open(...);`
	// and i want to compress it using a compression function, ex. `compress(stream_in, stream_out);`
	// then i want to encrypt it using an encryption function, ex. `encrypt(stream_in, stream_out);`
	// the normal usage would be to:
	// ```
	// auto file = file_open(...);
	// auto my_temporary_stream = get_desired_temporary_stream(...);
	// compress(file, my_temporary_stream);
	// auto my_output_stream = get_desired_output_stream(...);
	// encrypt(my_temporary_stream, my_output_stream);
	// ```
	// this is fine but it forces you to compress the file completely before encrypting it
	// and this file might be very large, it's desirable to process it without copying it in memory
	// this function allows your code to be like this:
	// ```
	// auto file = file_open(...)
	// auto compressed_stream = lazy_stream(fabric, compress, file);
	// auto encrypted_stream = lazy_stream(fabric, encrypt, compressed_stream);
	// auto my_output_stream = get_desired_output_stream(...);
	// copy_stream(encrypted_stream, my_output_stream);
	// ```
	// this way you won't need to load the entire file into memory in order to process it
	template<typename TFunc, typename ... TArgs>
	inline static Auto_Chan_Stream
	lazy_stream(Fabric f, TFunc&& func, mn::Stream stream_in, TArgs&& ... args)
	{
		Auto_Chan_Stream res;
		mn::go(f, [=]{
			func(stream_in, res, args...);
			chan_stream_close(res);
		});
		return res;
	}

	template<typename T>
	struct _IFuture
	{
		Waitgroup _wg;
		T result;
	};

	template<>
	struct _IFuture<void>
	{
		Waitgroup _wg;
	};

	// future is a wrapper around the result of any function which called in an async way using fabric
	// it's useful in case you want to track the completion of this function
	template<typename T>
	struct Future
	{
		_IFuture<T>* _internal_future;

		const T* operator->() const { return &_internal_future->result; }
		T* operator->() { return &_internal_future->result; }

		operator const T&() const { return _internal_future->result; }
		operator T&() { return _internal_future->result; }

		const T& operator*() const { return _internal_future->result; }
		T& operator*() { return _internal_future->result; }
	};

	template<>
	struct Future<void>
	{
		_IFuture<void>* _internal_future;
	};

	// schedules a function with the given arguments to be run on fabric, and returns the future of this
	// operation
	template<typename TFunc, typename ... TArgs>
	inline static Future<std::invoke_result_t<TFunc, TArgs...>>
	future_go(Fabric f, TFunc&& fn, TArgs&& ... args)
	{
		using return_type = std::invoke_result_t<TFunc, TArgs...>;
		Future<return_type> self{};
		self._internal_future = mn::alloc_zerod<_IFuture<return_type>>();
		self._internal_future->_wg = waitgroup_new();
		waitgroup_add(self._internal_future->_wg, 1);

		Fabric_Task entry{};
		entry.as_oneshot.task = Task<void()>::make([=]() mutable {
			if constexpr (std::is_same_v<return_type, void>)
				fn(args...);
			else
				self._internal_future->result = fn(args...);
			waitgroup_done(self._internal_future->_wg);
		});
		fabric_task_do(f, entry);

		return self;
	}

	// schedules a function with the given arguments to be run on worker, and returns the future of this
	// operation
	template<typename TFunc, typename ... TArgs>
	inline static Future<std::invoke_result_t<TFunc, TArgs...>>
	future_go(Worker w, TFunc&& fn, TArgs&& ... args)
	{
		using return_type = std::invoke_result_t<TFunc, TArgs...>;
		Future<return_type> self{};
		self._internal_future = mn::alloc_zerod<_IFuture<return_type>>();
		self._internal_future->_wg = waitgroup_new();
		waitgroup_add(self._internal_future->_wg, 1);

		Fabric_Task entry{};
		entry.as_oneshot.task = Task<void()>::make([=]() mutable {
			if constexpr (std::is_same_v<return_type, void>)
				fn(args...);
			else
				self._internal_future->result = fn(args...);
			waitgroup_done(self._internal_future->_wg);
		});
		worker_task_do(w, entry);

		return self;
	}

	// frees the given future, if it's not done yet we wait before freeing it
	template<typename T>
	inline static void
	future_free(Future<T>& self)
	{
		if (self._internal_future == nullptr)
			return;

		waitgroup_wait(self._internal_future->_wg);
		waitgroup_free(self._internal_future->_wg);

		if constexpr (std::is_same_v<T, void> == false)
			destruct(self._internal_future->result);

		free(self._internal_future);
		self._internal_future = nullptr;
	}

	// destruct overload for future
	template<typename T>
	inline static void
	destruct(Future<T>& self)
	{
		future_free(self);
	}

	// returns whether the given future has finished
	template<typename T>
	inline static bool
	future_is_done(Future<T> self)
	{
		return waitgroup_count(self._internal_future->_wg) == 0;
	}

	// waits for the given future to be done, in case it's done already we don't sleep/wait
	template<typename T>
	inline static void
	future_wait(Future<T> self)
	{
		waitgroup_wait(self._internal_future->_wg);
	}

	// returns whether the future is null/empty
	template<typename T>
	inline static bool
	future_is_null(Future<T> self)
	{
		return self._internal_future == nullptr;
	}

	// a generic message passing primitive used to communicate between fabric tasks
	template<typename T>
	struct IChan
	{
		Ring<T> r;
		Mutex mtx;
		Cond_Var read_cv;
		Cond_Var write_cv;
		std::atomic<int32_t> atomic_limit;
		std::atomic<int32_t> atomic_arc;
	};
	template<typename T>
	using Chan = IChan<T>*;

	// creates a new channel
	template<typename T>
	inline static Chan<T>
	chan_new(int32_t limit = 1)
	{
		mn_assert(limit != 0);
		Chan<T> self = alloc<IChan<T>>();

		self->r = ring_new<T>();
		ring_reserve(self->r, limit);
		self->mtx = mn_mutex_new_with_srcloc("Channel Mutex");
		self->read_cv = cond_var_new();
		self->write_cv = cond_var_new();
		self->atomic_limit = limit;
		self->atomic_arc = 1;
		return self;
	}

	// increments the reference count of the given channel
	// because this channel is used to communicate between threads, it follows that
	// its ownership model is not unique to single thread, but it's shared between multiple threads
	// that's why it uses a reference counting model for its ownership
	template<typename T>
	inline static Chan<T>
	chan_ref(Chan<T> self)
	{
		self->atomic_arc.fetch_add(1);
		return self;
	}

	// closes the given channel, which means that any subsquent writes will fail
	template<typename T>
	inline static void
	chan_close(Chan<T> self);

	// decrements the reference count of the given channel
	// because this channel is used to communicate between threads, it follows that
	// its ownership model is not unique to single thread, but it's shared between multiple threads
	// that's why it uses a reference counting model for its ownership
	template<typename T>
	inline static Chan<T>
	chan_unref(Chan<T> self)
	{
		if (self->atomic_arc.fetch_sub(1) == 1)
		{
			chan_close(self);

			destruct(self->r);
			mutex_free(self->mtx);
			cond_var_free(self->read_cv);
			cond_var_free(self->write_cv);
			mn::free(self);
			return nullptr;
		}
		return self;
	}

	// increments the reference count of the given channel and returns it
	// thus making a new instance of the same channel
	template<typename T>
	inline static Chan<T>
	chan_new(Chan<T> self)
	{
		chan_ref(self);
		return self;
	}

	// frees the given channel, by decrementing its reference count and only freeing it if it reaches 0
	template<typename T>
	inline static void
	chan_free(Chan<T> self)
	{
		chan_unref(self);
	}

	// destruct overload of channel free
	template<typename T>
	inline static void
	destruct(Chan<T> self)
	{
		chan_free(self);
	}

	// returns whether the given channel is closed
	template<typename T>
	inline static bool
	chan_closed(Chan<T> self)
	{
		return self->atomic_limit.load() == 0;
	}

	// closes the given channel, which means that any subsquent writes will fail
	template<typename T>
	inline static void
	chan_close(Chan<T> self)
	{
		mutex_lock(self->mtx);
		self->atomic_limit.exchange(0);
		mutex_unlock(self->mtx);
		cond_var_notify_all(self->read_cv);
		cond_var_notify_all(self->write_cv);
	}

	// checks whether you can send to the given channel
	template<typename T>
	inline static bool
	chan_can_send(Chan<T> self)
	{
		mutex_lock(self->mtx);
		bool res = (self->r.count < size_t(self->atomic_limit.load())) && (chan_closed(self) == false);
		mutex_unlock(self->mtx);
		return res;
	}

	// tries to send the given value to the channel and returns whether it succeeded or not
	template<typename T>
	inline static bool
	chan_send_try(Chan<T> self, const T& v)
	{
		mutex_lock(self->mtx);

		bool notify = false;
		mn_defer
		{
			mutex_unlock(self->mtx);
			if (notify)
				cond_var_notify(self->read_cv);
		};

		if (self->r.count < self->atomic_limit)
		{
			ring_push_back(self->r, v);
			notify = true;
			return true;
		}
		return false;
	}

	// sends the given value to the channel, if it doesn't succeed it will block until it the value is sent
	template<typename T>
	inline static void
	chan_send(Chan<T> self, const T& v)
	{
		chan_ref(self);
		mn_defer{chan_unref(self);};

		mutex_lock(self->mtx);
		cond_var_wait(self->write_cv, self->mtx, [self] {
			return (self->r.count < size_t(self->atomic_limit.load()) ||
					chan_closed(self));
		});

		if (chan_closed(self))
			panic("cannot send in a closed channel");

		ring_push_back(self->r, v);
		mutex_unlock(self->mtx);

		cond_var_notify(self->read_cv);
	}

	// checks whether you can recieve from the given channel
	template<typename T>
	inline static bool
	chan_can_recv(Chan<T> self)
	{
		mutex_lock(self->mtx);
			bool res = (self->r.count > 0) && (chan_closed(self) == false);
		mutex_unlock(self->mtx);
		return res;
	}

	// represents the return of the channel recieve operation
	template<typename T>
	struct Recv_Result
	{
		// the actualy recieved value
		T res;
		// boolean to indicate whether the recieve operation succeeded
		bool more;
	};

	// tries to recieve a value from the given channel
	template<typename T>
	inline static Recv_Result<T>
	chan_recv_try(Chan<T> self)
	{
		mutex_lock(self->mtx);

		bool notify = false;
		mn_defer
		{
			mutex_unlock(self->mtx);
			if (notify)
				cond_var_notify(self->write_cv);
		};

		if (self->r.count > 0)
		{
			T res = ring_front(self->r);
			ring_pop_front(self->r);
			notify = true;
			return { res, true };
		}
		return { T{}, false };
	}

	// recieves a value from the given channel, it doesn't succeed it will block until a value is recieved
	template<typename T>
	inline static Recv_Result<T>
	chan_recv(Chan<T> self)
	{
		chan_ref(self);
		mn_defer{chan_unref(self);};

		mutex_lock(self->mtx);
		cond_var_wait(self->read_cv, self->mtx, [self] {
			return self->r.count > 0 || chan_closed(self);
		});

		if(self->r.count > 0)
		{
			T res = ring_front(self->r);
			ring_pop_front(self->r);
			mutex_unlock(self->mtx);

			cond_var_notify(self->write_cv);
			return { res, true };
		}
		else if(chan_closed(self))
		{
			mutex_unlock(self->mtx);
			return { T{}, false };
		}
		else
		{
			panic("unreachable");
		}
	}

	// an iterator wrapper over the channel which allows you to use it in a range for loop
	// `for (auto value: my_channel)`
	template<typename T>
	struct Chan_Iterator
	{
		Chan<T> chan;
		Recv_Result<T> res;

		Chan_Iterator&
		operator++()
		{
			res = chan_recv(chan);
			return *this;
		}

		Chan_Iterator
		operator++(int)
		{
			auto r = *this;
			res = chan_recv(chan);
			return r;
		}

		bool
		operator==(const Chan_Iterator& other) const
		{
			return chan == other.chan && res.more == other.res.more;
		}

		bool
		operator!=(const Chan_Iterator& other) const
		{
			return !operator==(other);
		}

		T&
		operator*()
		{
			return res.res;
		}

		const T&
		operator*() const
		{
			return res.res;
		}

		T*
		operator->()
		{
			return &res.res;
		}

		const T*
		operator->() const
		{
			return &res.res;
		}
	};

	// begin overload of the channel which allows its use in range for loops
	template<typename T>
	inline static Chan_Iterator<T>
	begin(Chan<T> self)
	{
		return Chan_Iterator<T>{self, chan_recv(self)};
	}

	// end overload of the channel which allows its use in range for loops
	template<typename T>
	inline static Chan_Iterator<T>
	end(Chan<T> self)
	{
		return Chan_Iterator<T>{self, {}};
	}

	// automatic wrapper around channel which uses RAII to handle the reference counting
	// useful for scoped usage of channels
	template<typename T>
	struct Auto_Chan
	{
		Chan<T> handle;

		explicit Auto_Chan(int32_t limit = 1)
			: handle(chan_new<T>(limit))
		{}

		Auto_Chan(const Auto_Chan& other)
			: handle(chan_new(other.handle))
		{}

		Auto_Chan(Auto_Chan&& other)
			: handle(other.handle)
		{
			other.handle = nullptr;
		}

		Auto_Chan&
		operator=(const Auto_Chan& other)
		{
			if (handle) chan_free(handle);
			handle = chan_new(other.handle);
			return *this;
		}

		Auto_Chan&
		operator=(Auto_Chan&& other)
		{
			if (handle) chan_free(handle);
			handle = other.handle;
			other.handle = nullptr;
			return *this;
		}

		~Auto_Chan()
		{
			if (handle) chan_free(handle);
		}

		operator Chan<T>() const { return handle; }
		Chan_Iterator<T> begin() { return Chan_Iterator<T>{handle, chan_recv(handle)}; }
		Chan_Iterator<T> end() { return Chan_Iterator<T>{handle, {}}; }
	};

	// returns whether the given automatic channel is closed
	template<typename T>
	inline static bool
	chan_closed(const Auto_Chan<T> &self)
	{
		return chan_closed(self.handle);
	}

	// closes the given automatic channel
	template<typename T>
	inline static void
	chan_close(Auto_Chan<T> &self)
	{
		chan_close(self.handle);
	}

	// checks whether the given automatic channel can send
	template<typename T>
	inline static bool
	chan_can_send(const Auto_Chan<T> &self)
	{
		return chan_can_send(self.handle);
	}

	// tries to send a value to the given automatic channel
	template<typename T>
	inline static bool
	chan_send_try(Auto_Chan<T> &self, const T& v)
	{
		return chan_send_try(self.handle, v);
	}

	// sends a value to the given automatic channel, if it doesn't succeed it will block until the value is sent
	template<typename T>
	inline static void
	chan_send(Auto_Chan<T> &self, const T& v)
	{
		return chan_send(self.handle, v);
	}

	// returns whether you can recieve from the given automatic channel
	template<typename T>
	inline static bool
	chan_can_recv(const Auto_Chan<T> &self)
	{
		return chan_can_recv(self.handle);
	}

	// tries to recieve from the given automatic channel
	template<typename T>
	inline static Recv_Result<T>
	chan_recv_try(Auto_Chan<T> &self)
	{
		return chan_recv_try(self.handle);
	}

	// recieves a value from the given automatic channel, if it doesn't succeed it will block until a value is recieved
	template<typename T>
	inline static Recv_Result<T>
	chan_recv(Auto_Chan<T> &self)
	{
		return chan_recv(self.handle);
	}

	template<typename TFunc>
	inline static void
	_single_threaded_compute(Compute_Dims workgroup_num, Compute_Dims total_size, Compute_Dims tile_size, TFunc&& fn)
	{
		for (size_t global_z = 0; global_z < workgroup_num.z; ++global_z)
		{
			for (size_t global_y = 0; global_y < workgroup_num.y; ++global_y)
			{
				for (size_t global_x = 0; global_x < workgroup_num.x; ++global_x)
				{
					auto checkpoint = mn::memory::tmp()->checkpoint();
					mn_defer{mn::memory::tmp()->restore(checkpoint);};

					Compute_Args args{};
					args.workgroup_size = tile_size;
					args.workgroup_num = workgroup_num;
					args.workgroup_id = Compute_Dims{ global_x, global_y, global_z };
					// workgroup_id * workgroup_size + local_invocation_id
					args.global_invocation_id = Compute_Dims{
						global_x * tile_size.x,
						global_y * tile_size.y,
						global_z * tile_size.z
					};
					args.tile_size = tile_size;
					if (args.tile_size.x + args.global_invocation_id.x >= total_size.x)
						args.tile_size.x = total_size.x - args.global_invocation_id.x;
					if (args.tile_size.y + args.global_invocation_id.y >= total_size.y)
						args.tile_size.y = total_size.y - args.global_invocation_id.y;
					if (args.tile_size.z + args.global_invocation_id.z >= total_size.z)
						args.tile_size.z = total_size.z - args.global_invocation_id.z;
					fn(args);
				}
			}
		}
	}

	template<typename TFunc>
	inline static void
	_multi_threaded_compute(Fabric self, Compute_Dims workgroup_num, Compute_Dims total_size, Compute_Dims tile_size, TFunc&& fn)
	{
		auto batch = buf_with_allocator<Fabric_Task>(memory::tmp());

		Auto_Waitgroup wg;
		for (size_t global_z = 0; global_z < workgroup_num.z; ++global_z)
		{
			for (size_t global_y = 0; global_y < workgroup_num.y; ++global_y)
			{
				for (size_t global_x = 0; global_x < workgroup_num.x; ++global_x)
				{
					Fabric_Task entry{};
					entry.kind = Fabric_Task::KIND_COMPUTE;
					entry.as_compute.args.workgroup_size = tile_size;
					entry.as_compute.args.workgroup_num = workgroup_num;
					entry.as_compute.args.workgroup_id = Compute_Dims{ global_x, global_y, global_z };
					// workgroup_id * workgroup_size + local_invocation_id
					entry.as_compute.args.global_invocation_id = Compute_Dims{
						global_x * tile_size.x,
						global_y * tile_size.y,
						global_z * tile_size.z
					};
					entry.as_compute.args.tile_size = tile_size;
					if (entry.as_compute.args.tile_size.x + entry.as_compute.args.global_invocation_id.x >= total_size.x)
						entry.as_compute.args.tile_size.x = total_size.x - entry.as_compute.args.global_invocation_id.x;
					if (entry.as_compute.args.tile_size.y + entry.as_compute.args.global_invocation_id.y >= total_size.y)
						entry.as_compute.args.tile_size.y = total_size.y - entry.as_compute.args.global_invocation_id.y;
					if (entry.as_compute.args.tile_size.z + entry.as_compute.args.global_invocation_id.z >= total_size.z)
						entry.as_compute.args.tile_size.z = total_size.z - entry.as_compute.args.global_invocation_id.z;
					entry.as_compute.task = Task<void(Compute_Args)>::make(fn);
					entry.as_compute.wg = wg.handle;
					buf_push(batch, entry);
				}
			}
		}

		wg.add((int)batch.count);
		fabric_task_batch_do(self, batch.ptr, batch.count);
		wg.wait();
	}

	// performs the compute function in tiles so if you have a total size of
	// (100, 100, 100) and tile size of (10, 10, 10) you get (10, 10, 10) = 1000 workgroups
	// but a single local worker for the (10, 10, 10) step/tile
	// so basically your function will be called total_size/step_size number of times
	// and will not be invoked for each local tile individually so you have
	// to process the entire tile in the single call
	template<typename TFunc>
	inline static void
	compute(Fabric f, Compute_Dims total_size, Compute_Dims tile_size, TFunc&& fn)
	{
		Compute_Dims workgroup_num{
			1 + ((total_size.x - 1) / tile_size.x),
			1 + ((total_size.y - 1) / tile_size.y),
			1 + ((total_size.z - 1) / tile_size.z)
		};
		if (f == nullptr)
			_single_threaded_compute(workgroup_num, total_size, tile_size, std::forward<TFunc>(fn));
		else
			_multi_threaded_compute(f, workgroup_num, total_size, tile_size, std::forward<TFunc>(fn));
	}
}