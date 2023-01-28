#include "mn/Fabric.h"
#include "mn/Memory.h"
#include "mn/Pool.h"
#include "mn/Buf.h"
#include "mn/Log.h"
#include "mn/Assert.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace mn
{
	constexpr static auto DEFAULT_COOP_BLOCKING_THRESHOLD = 10;
	constexpr static auto DEFAULT_EXTR_BLOCKING_THRESHOLD = 1000;

	// Worker
	struct IWorker
	{
		enum STATE
		{
			STATE_RUNNING,
			STATE_PAUSED,
			STATE_STOP_REQUEST,
			STATE_STOP_ACKNOWLEDGED
		};

		Str name;
		Mutex mtx;
		Cond_Var cv;
		Fabric fabric;
		Ring<Fabric_Task> job_q;
		Thread thread;
		// index within a fabric
		size_t fabric_index;
		std::atomic<Fabric_Task::KIND> atomic_current_job_kind;
		std::atomic<uint64_t> atomic_job_start_time_in_ms;
		std::atomic<uint64_t> atomic_block_start_time_in_ms;
		std::atomic<STATE> atomic_state;
		std::atomic<bool> atomic_disable_block_timing;
	};
	thread_local Worker LOCAL_WORKER = nullptr;

	struct IFabric_Timer
	{
		Task<void()> task;
		uint32_t milliseconds;
		uint32_t remaining;
		bool is_single_shot;
		bool free_after_single_shot;
		bool stopped;
		bool atomic_freed;
		std::atomic<int> running_instances;
	};

	struct IFabric_Timer_System
	{
		Mutex mtx;
		Pool timers_pool;
		Map<int, IFabric_Timer*> timers;
		std::atomic<int> atomic_timer_jobs_count;
		std::atomic<int> atomic_timer_id_generator;
	};

	struct IFabric
	{
		Fabric_Settings settings;
		Str name;
		Str sysmon_name;

		Buf<Worker> workers;
		Buf<Worker> sleepy_side_workers;
		Buf<Worker> ready_side_workers;

		Mutex mtx;
		Cond_Var cv;
		bool is_running;
		std::atomic<size_t> atomic_available_jobs;
		size_t next_worker;
		size_t worker_id_generator;

		IFabric_Timer_System timer_system;

		Thread sysmon;
	};

	static void
	_worker_main(void* worker)
	{
		auto self = (Worker)worker;
		LOCAL_WORKER = self;

		if (self->fabric)
			if (self->fabric->settings.on_worker_start)
				self->fabric->settings.on_worker_start();

		while(true)
		{
			auto state = self->atomic_state.load();
			if (state == IWorker::STATE_RUNNING)
			{
				Fabric_Task job{};
				{
					mutex_lock(self->mtx);
					mn_defer{mutex_unlock(self->mtx);};

					if (self->job_q.count == 0)
					{
						cond_var_wait(self->cv, self->mtx, [&]{
							return self->job_q.count > 0 ||
								self->atomic_state.load() != IWorker::STATE_RUNNING;
						});
						state = self->atomic_state.load();
					}

					if (state != IWorker::STATE_RUNNING)
						continue;

					job = ring_front(self->job_q);
					ring_pop_front(self->job_q);
				}

				self->atomic_job_start_time_in_ms.store(time_in_millis());
				self->atomic_disable_block_timing = false;
				self->atomic_current_job_kind.store(job.kind);
				if (job.kind == Fabric_Task::KIND_TIMER)
				{
					job.as_timer->task();
				}
				else
				{
					fabric_task_run(job);
				}
				self->atomic_disable_block_timing = true;
				self->atomic_job_start_time_in_ms.store(0);
				self->atomic_current_job_kind.store(Fabric_Task::KIND_ONESHOT);
				if (job.kind == Fabric_Task::KIND_TIMER)
				{
					// we don't free timer tasks because they are owned by fabric itself
					[[maybe_unused]] auto count = job.as_timer->running_instances.fetch_sub(1);
					mn_assert(count > 0);
				}
				else
				{
					fabric_task_free(job);
				}
				memory::tmp()->clear_all();
				if (self->fabric)
				{
					if (self->fabric->settings.after_each_job)
						self->fabric->settings.after_each_job();
					self->fabric->atomic_available_jobs.fetch_sub(1);
				}
			}
			else if (state == IWorker::STATE_PAUSED)
			{
				mutex_lock(self->mtx);
				mn_defer{mutex_unlock(self->mtx);};

				if (self->atomic_state.load() == IWorker::STATE_PAUSED)
				{
					cond_var_wait(self->cv, self->mtx, [&]{
						return self->atomic_state.load() != IWorker::STATE_PAUSED;
					});
				}
			}
			else if (state == IWorker::STATE_STOP_REQUEST)
			{
				break;
			}
			else
			{
				mn_unreachable();
			}
		}

		[[maybe_unused]] auto old_state = self->atomic_state.exchange(IWorker::STATE_STOP_ACKNOWLEDGED);
		mn_assert(old_state == IWorker::STATE_STOP_REQUEST);
		LOCAL_WORKER = nullptr;
	}

	inline static void
	_worker_stop(Worker self)
	{
		mutex_lock(self->mtx);
		mn_defer{mutex_unlock(self->mtx);};

		self->atomic_state = IWorker::STATE_STOP_REQUEST;
		cond_var_notify(self->cv);
	}

	inline static void
	_worker_pause(Worker self)
	{
		mutex_lock(self->mtx);
		mn_defer{mutex_unlock(self->mtx);};

		mn_assert(self->atomic_state == IWorker::STATE_RUNNING);

		self->atomic_state = IWorker::STATE_PAUSED;
		cond_var_notify(self->cv);
	}

	inline static void
	_worker_resume(Worker self)
	{
		mutex_lock(self->mtx);
		mn_defer{mutex_unlock(self->mtx);};

		mn_assert(self->atomic_state == IWorker::STATE_PAUSED);

		self->atomic_state = IWorker::STATE_RUNNING;
		cond_var_notify(self->cv);
	}

	inline static Worker
	_worker_new(Str name, Fabric fabric, size_t fabric_index = 0, Ring<Fabric_Task> stolen_jobs = ring_new<Fabric_Task>())
	{
		auto self = alloc_zerod<IWorker>();
		self->name = name;
		self->mtx = mn_mutex_new_with_srcloc(self->name.ptr);
		self->cv = cond_var_new();
		self->fabric = fabric;
		self->job_q = stolen_jobs;
		self->fabric_index = fabric_index;
		self->atomic_state = IWorker::STATE_RUNNING;
		self->atomic_disable_block_timing = true;
		self->thread = thread_new(_worker_main, self, self->name.ptr);
		return self;
	}

	inline static void
	_worker_free(Worker self)
	{
		[[maybe_unused]] auto state = self->atomic_state.load();
		mn_assert(state == IWorker::STATE_STOP_REQUEST ||
			   state == IWorker::STATE_STOP_ACKNOWLEDGED);

		thread_join(self->thread);
		thread_free(self->thread);

		str_free(self->name);
		mutex_free(self->mtx);
		cond_var_free(self->cv);
		destruct(self->job_q);

		free(self);
	}

	// Fabric
	inline static void
	_sysmon_detect_blocking_workers(Fabric self, Buf<Worker>& blocking_workers)
	{
		// detect blocking workers
		for (auto worker: self->workers)
		{
			auto current_job_flags = worker->atomic_current_job_kind.load();
			auto block_start_time = worker->atomic_block_start_time_in_ms.load();
			if(block_start_time != 0 && current_job_flags == Fabric_Task::KIND_ONESHOT)
			{
				auto block_time = time_in_millis() - block_start_time;
				if(block_time > self->settings.coop_blocking_threshold_in_ms)
				{
					buf_push(blocking_workers, worker);
				}
			}
		}

		// if we have some free workers then it's okay, ignore it this is normal
		// we only care about total system blocking
		if (blocking_workers.count < self->workers.count * self->settings.blocking_workers_threshold)
			buf_clear(blocking_workers);

		// pause all the blocking workers
		for (auto blocking_worker: blocking_workers)
			_worker_pause(blocking_worker);

		// move the blocking workers out
		{
			mutex_lock(self->mtx);
			mn_defer{mutex_unlock(self->mtx);};

			for (auto blocking_worker: blocking_workers)
			{
				Ring<Fabric_Task> job_q{};
				{
					mutex_lock(blocking_worker->mtx);
					mn_defer{mutex_unlock(blocking_worker->mtx);};

					job_q = blocking_worker->job_q;
					blocking_worker->job_q = ring_new<Fabric_Task>();
				}

				{
					// find a suitable worker
					if (self->ready_side_workers.count > 0)
					{
						auto new_worker = buf_top(self->ready_side_workers);
						buf_pop(self->ready_side_workers);

						new_worker->fabric_index = blocking_worker->fabric_index;
						self->workers[blocking_worker->fabric_index] = new_worker;
						mn_assert(new_worker->job_q.count == 0);
						ring_free(new_worker->job_q);
						new_worker->job_q = job_q;

						_worker_resume(new_worker);
					}
					else
					{
						auto new_worker = _worker_new(
							strf("{} worker #{}", self->name, self->worker_id_generator++),
							self,
							blocking_worker->fabric_index,
							job_q
						);

						self->workers[blocking_worker->fabric_index] = new_worker;
					}
				}
			}
		}

		// now that we have replaced all the blocking workers with a newly created workers
		// we need to store the blocking workers away to be reused later in the replacement above
		for (auto blocking_worker: blocking_workers)
			buf_push(self->sleepy_side_workers, blocking_worker);

		// clear the blocking workers list
		buf_clear(blocking_workers);
	}

	inline static void
	_sysmon_detect_long_running_workers(Fabric self, Buf<Worker>& blocking_workers)
	{
		// detect blocking workers
		for (auto worker: self->workers)
		{
			auto current_job_flags = worker->atomic_current_job_kind.load();
			auto job_start_time = worker->atomic_job_start_time_in_ms.load();
			if (job_start_time != 0 && current_job_flags == Fabric_Task::KIND_ONESHOT)
			{
				auto job_run_time = time_in_millis() - job_start_time;
				if(job_run_time > self->settings.external_blocking_threshold_in_ms)
				{
					buf_push(blocking_workers, worker);
				}
			}
		}

		// pause all the blocking workers
		for (auto blocking_worker: blocking_workers)
			_worker_pause(blocking_worker);

		// move the blocking workers out
		{
			mutex_lock(self->mtx);
			mn_defer{mutex_unlock(self->mtx);};

			for (auto blocking_worker: blocking_workers)
			{
				Ring<Fabric_Task> job_q{};
				{
					mutex_lock(blocking_worker->mtx);
					mn_defer{mutex_unlock(blocking_worker->mtx);};

					job_q = blocking_worker->job_q;
					blocking_worker->job_q = ring_new<Fabric_Task>();
				}

				{
					// find a suitable worker
					if (self->ready_side_workers.count > 0)
					{
						auto new_worker = buf_top(self->ready_side_workers);
						buf_pop(self->ready_side_workers);

						new_worker->fabric_index = blocking_worker->fabric_index;
						self->workers[blocking_worker->fabric_index] = new_worker;
						mn_assert(new_worker->job_q.count == 0);
						ring_free(new_worker->job_q);
						new_worker->job_q = job_q;

						_worker_resume(new_worker);
					}
					else
					{
						auto new_worker = _worker_new(
							strf("{} worker #{}", self->name, self->worker_id_generator++),
							self,
							blocking_worker->fabric_index,
							job_q
						);

						self->workers[blocking_worker->fabric_index] = new_worker;
					}
				}
			}
		}

		// now that we have replaced all the blocking workers with a newly created workers
		// we need to store the blocking workers away to be reused later in the replacement above
		for (auto blocking_worker: blocking_workers)
			buf_push(self->sleepy_side_workers, blocking_worker);

		// clear the blocking workers list
		buf_clear(blocking_workers);
	}

	static void
	_sysmon_main(void* fabric)
	{
		_disable_profiling_for_this_thread();

		auto self = (Fabric)fabric;

		// workers who exceeded the coop blocking threshold
		auto blocking_workers = buf_with_capacity<Worker>(self->workers.count);
		mn_defer{buf_free(blocking_workers);};

		// workers who exceeded the external blocking threshold
		auto long_running_workers = buf_with_capacity<Worker>(self->workers.count);
		mn_defer{buf_free(long_running_workers);};

		auto dead_workers = buf_with_capacity<Worker>(self->workers.count);
		mn_defer{destruct(dead_workers);};

		auto tmp_jobs = buf_new<Fabric_Task>();
		mn_defer{destruct(tmp_jobs);};

		auto timeslice = self->settings.coop_blocking_threshold_in_ms;
		if (timeslice > self->settings.external_blocking_threshold_in_ms)
			timeslice = self->settings.external_blocking_threshold_in_ms;

		timeslice /= 2;
		if (timeslice == 0)
			timeslice = 1;

		uint32_t timer_min_sleep_in_ms = 0;
		auto timer_prev = time_in_millis();

		auto removed_timers = buf_new<int>();
		mn_defer{buf_free(removed_timers);};

		while(true)
		{
			// dispose of dead workers before holding the mutex
			buf_remove_if(dead_workers, [](Worker worker){
				auto state = worker->atomic_state.load();

				if (state == IWorker::STATE_STOP_REQUEST)
					return false;

				mn_assert(state == IWorker::STATE_STOP_ACKNOWLEDGED);
				_worker_free(worker);
				return true;
			});

			bool slept_on_cond_var = false;

			{
				mutex_lock(self->mtx);
				mn_defer{mutex_unlock(self->mtx);};

				if (self->atomic_available_jobs.load() == 0 &&
					self->sleepy_side_workers.count == 0 &&
					self->timer_system.atomic_timer_jobs_count.load() == 0)
				{
					slept_on_cond_var = true;
					cond_var_wait(self->cv, self->mtx, [&]{
						return self->atomic_available_jobs.load() > 0 ||
							self->is_running == false ||
							self->sleepy_side_workers.count > 0 ||
							self->timer_system.atomic_timer_jobs_count.load();
					});
				}

				if (self->is_running == false)
					return;
			}

			// SYSMON rest station, sysmon needs to sleep for some time, he does a lot of work, he deserves it
			if (slept_on_cond_var == false)
			{
				auto timer_now = time_in_millis();
				auto timer_delta_in_ms = timer_now - timer_prev;
				if (timer_delta_in_ms > timer_min_sleep_in_ms)
					timer_min_sleep_in_ms = 0;
				else
					timer_min_sleep_in_ms -= (uint32_t)timer_delta_in_ms;

				auto sleep_time_in_ms = timeslice < timer_min_sleep_in_ms ? timeslice : timer_min_sleep_in_ms;

				if (sleep_time_in_ms > 0)
					thread_sleep(sleep_time_in_ms);
			}

			{
				mutex_lock(self->timer_system.mtx);
				mn_defer{mutex_unlock(self->timer_system.mtx);};

				auto timer_now = time_in_millis();
				auto timer_delta_in_ms = timer_now - timer_prev;
				timer_prev = timer_now;

				buf_clear(removed_timers);
				for (auto& [key, timer]: self->timer_system.timers)
				{
					if (timer->atomic_freed && timer->running_instances.load() == 0)
					{
						task_free(timer->task);
						--self->timer_system.atomic_timer_id_generator;
						buf_push(removed_timers, key);
					}
				}

				for (auto t: removed_timers)
				{
					map_remove(self->timer_system.timers, t);
				}

				timer_min_sleep_in_ms = UINT32_MAX;
				for (auto& [_, timer]: self->timer_system.timers)
				{
					if (timer->stopped)
					{
						timer->remaining = timer->milliseconds;
						continue;
					}

					if (timer_delta_in_ms > timer->remaining)
					{
						timer->remaining = 0;
					}
					else
					{
						timer->remaining -= (uint32_t)timer_delta_in_ms;
					}

					if (timer->remaining == 0)
					{
						timer->running_instances.fetch_add(1);

						auto next_worker = self->next_worker++;
						next_worker %= self->workers.count;

						auto worker = self->workers[next_worker];

						Fabric_Task task{};
						task.kind = Fabric_Task::KIND_TIMER;
						task.as_timer = timer;
						worker_task_do(worker, task);
						timer->remaining = timer->milliseconds;

						if (timer->is_single_shot)
							timer->stopped = true;
						if (timer->free_after_single_shot)
							timer->atomic_freed = true;
					}

					timer_min_sleep_in_ms = timer_min_sleep_in_ms > timer->remaining ? timer->remaining : timer_min_sleep_in_ms;
				}
			}

			// get the max/min jobs
			size_t busiest_worker = 0;
			size_t max_jobs = 0;
			size_t idle_worker = SIZE_MAX;
			size_t min_jobs = SIZE_MAX;
			for (size_t i = 0; i < self->workers.count; ++i)
			{
				auto worker = self->workers[i];

				mutex_lock(worker->mtx);
				mn_defer{mutex_unlock(worker->mtx);};

				if (worker->job_q.count > max_jobs)
				{
					busiest_worker = i;
					max_jobs = worker->job_q.count;
				}

				if (worker->atomic_job_start_time_in_ms.load() == 0 && worker->job_q.count < min_jobs)
				{
					idle_worker = i;
					min_jobs = worker->job_q.count;
				}
			}

			// steal from the max jobs and give to min jobs, if we have a worker that's idle
			if (idle_worker < self->workers.count && max_jobs > min_jobs && min_jobs == 0)
			{
				{
					auto max_worker = self->workers[busiest_worker];

					mutex_lock(max_worker->mtx);
					mn_defer{mutex_unlock(max_worker->mtx);};

					max_jobs = max_worker->job_q.count;
					size_t job_steal_count = max_jobs;
					if (job_steal_count > 1)
						job_steal_count /= 2;

					for (size_t i = 0; i < job_steal_count; ++i)
					{
						auto job = ring_back(max_worker->job_q);
						ring_pop_back(max_worker->job_q);

						buf_push(tmp_jobs, job);
					}
				}

				{
					auto min_worker = self->workers[idle_worker];

					mutex_lock(min_worker->mtx);
					mn_defer{mutex_unlock(min_worker->mtx);};

					for (auto job: tmp_jobs)
						ring_push_back(min_worker->job_q, job);
					buf_clear(tmp_jobs);

					cond_var_notify(min_worker->cv);
				}
			}

			// check if any sleepy worker is ready and move it either to the ready workers list
			// or free it because we don't really need it
			buf_remove_if(self->sleepy_side_workers, [self, &dead_workers](Worker worker) {
				if (worker->atomic_job_start_time_in_ms.load() == 0)
				{
					if (self->ready_side_workers.count < self->settings.put_aside_worker_count)
					{
						buf_push(self->ready_side_workers, worker);
					}
					else
					{
						_worker_stop(worker);
						buf_push(dead_workers, worker);
					}
					return true;
				}
				return false;
			});

			_sysmon_detect_blocking_workers(self, blocking_workers);
			_sysmon_detect_long_running_workers(self, long_running_workers);
		}
	}


	// API
	Fabric_Timer
	fabric_timer_new(Fabric fabric, const Task<void()>& task, uint32_t milliseconds, bool single_shot, bool free_after_single_shot)
	{
		Fabric_Timer self{};
		self.fabric = fabric;
		self.id = ++fabric->timer_system.atomic_timer_id_generator;

		{
			mutex_lock(fabric->timer_system.mtx);

			auto timer = (IFabric_Timer*)pool_get(fabric->timer_system.timers_pool);
			::memset((char*)timer, 0, sizeof(*timer));
			timer->task = task;
			timer->milliseconds = milliseconds;
			timer->remaining = timer->milliseconds;
			timer->is_single_shot = single_shot;
			timer->free_after_single_shot = free_after_single_shot;

			map_insert(fabric->timer_system.timers, self.id, timer);
			++fabric->timer_system.atomic_timer_jobs_count;

			mutex_unlock(fabric->timer_system.mtx);

			cond_var_notify(fabric->cv);
		}

		return self;
	}

	void
	fabric_timer_free(Fabric_Timer& self)
	{
		auto fabric = self.fabric;
		mutex_lock(fabric->timer_system.mtx);
		auto it = map_lookup(fabric->timer_system.timers, self.id);
		it->value->atomic_freed = true;
		mutex_unlock(fabric->timer_system.mtx);
	}

	void
	fabric_timer_start(Fabric_Timer& self)
	{
		auto fabric = self.fabric;
		mutex_lock(fabric->timer_system.mtx);
		auto it = map_lookup(fabric->timer_system.timers, self.id);
		it->value->stopped = false;
		it->value->remaining = it->value->milliseconds;
		mutex_unlock(fabric->timer_system.mtx);
	}

	void
	fabric_timer_stop(Fabric_Timer& self)
	{
		auto fabric = self.fabric;
		mutex_lock(fabric->timer_system.mtx);
		auto it = map_lookup(fabric->timer_system.timers, self.id);
		it->value->stopped = true;
		mutex_unlock(fabric->timer_system.mtx);
	}

	bool
	fabric_timer_is_single_shot(Fabric_Timer& self)
	{
		auto fabric = self.fabric;
		mutex_lock(fabric->timer_system.mtx);
		auto it = map_lookup(fabric->timer_system.timers, self.id);
		auto res = it->value->is_single_shot;
		mutex_unlock(fabric->timer_system.mtx);
		return res;
	}

	void
	fabric_timer_set_single_shot(Fabric_Timer& self, bool is_single_shot)
	{
		auto fabric = self.fabric;
		mutex_lock(fabric->timer_system.mtx);
		auto it = map_lookup(fabric->timer_system.timers, self.id);
		it->value->is_single_shot = is_single_shot;
		mutex_unlock(fabric->timer_system.mtx);
	}

	Worker
	worker_new(const char* name)
	{
		return _worker_new(str_from_c(name), nullptr);
	}

	void
	worker_free(Worker self)
	{
		_worker_stop(self);
		_worker_free(self);
	}

	void
	worker_task_do(Worker self, const Fabric_Task& task)
	{
		mutex_lock(self->mtx);
		mn_defer{mutex_unlock(self->mtx);};

		ring_push_back(self->job_q, task);
		cond_var_notify(self->cv);
	}

	void
	worker_task_batch_do(Worker self, const Fabric_Task* ptr, size_t count)
	{
		mutex_lock(self->mtx);
		mn_defer{mutex_unlock(self->mtx);};

		ring_reserve(self->job_q, count);
		for (size_t i = 0; i < count; ++i)
			ring_push_back(self->job_q, ptr[i]);
		cond_var_notify(self->cv);
	}

	Worker
	worker_local()
	{
		return LOCAL_WORKER;
	}

	void
	worker_block_ahead()
	{
		if (LOCAL_WORKER == nullptr)
			return;

		if (LOCAL_WORKER->atomic_disable_block_timing.load() == true)
			return;

		LOCAL_WORKER->atomic_block_start_time_in_ms.store(time_in_millis());
	}

	void
	worker_block_clear()
	{
		if (LOCAL_WORKER == nullptr)
			return;

		if (LOCAL_WORKER->atomic_disable_block_timing.load() == true)
			return;

		LOCAL_WORKER->atomic_block_start_time_in_ms.store(0);
	}

	int
	local_worker_index()
	{
		if (LOCAL_WORKER == nullptr)
			return -1;

		return (int)LOCAL_WORKER->fabric_index;
	}


	// fabric
	Fabric
	fabric_new(Fabric_Settings settings)
	{
		if (settings.name == nullptr)
			settings.name = "fabric";

		if (settings.workers_count == 0)
			settings.workers_count = std::thread::hardware_concurrency();
		if (settings.coop_blocking_threshold_in_ms == 0)
			settings.coop_blocking_threshold_in_ms = DEFAULT_COOP_BLOCKING_THRESHOLD;
		if (settings.external_blocking_threshold_in_ms == 0)
			settings.external_blocking_threshold_in_ms = DEFAULT_EXTR_BLOCKING_THRESHOLD;
		if (settings.put_aside_worker_count == 0)
			settings.put_aside_worker_count = settings.workers_count / 2;
		if (settings.blocking_workers_threshold == 0.0f)
			settings.blocking_workers_threshold = 0.5f;


		auto self = alloc_zerod<IFabric>();
		self->settings = settings;
		self->name = strf("{}", settings.name);
		self->sysmon_name = strf("{} sysmon thread", settings.name);
		self->workers = buf_with_count<Worker>(self->settings.workers_count);
		self->sleepy_side_workers = buf_new<Worker>();
		self->ready_side_workers = buf_new<Worker>();
		self->mtx = mn_mutex_new_with_srcloc(self->name.ptr);
		self->cv = cond_var_new();
		self->is_running = true;
		self->atomic_available_jobs = 0;
		self->next_worker = 0;
		self->worker_id_generator = 0;

		self->timer_system.mtx = mn_mutex_new_with_srcloc("fabric timer system");
		self->timer_system.timers_pool = pool_new(sizeof(IFabric_Timer), 128);

		for (size_t i = 0; i < self->workers.count; ++i)
		{
			self->workers[i] = _worker_new(
				strf("{} worker #{}", self->name, self->worker_id_generator++),
				self,
				i
			);
		}

		self->sysmon = thread_new(_sysmon_main, self, self->sysmon_name.ptr);

		return self;
	}

	void
	fabric_free(Fabric self)
	{
		{
			mutex_lock(self->mtx);
			mn_defer{mutex_unlock(self->mtx);};

			self->is_running = false;
			cond_var_notify(self->cv);
		}

		for (auto worker : self->workers)
			_worker_stop(worker);

		for (auto worker : self->sleepy_side_workers)
			_worker_stop(worker);

		for (auto worker : self->ready_side_workers)
			_worker_stop(worker);

		thread_join(self->sysmon);
		thread_free(self->sysmon);

		for (auto worker : self->workers)
			_worker_free(worker);
		buf_free(self->workers);

		for (auto worker : self->sleepy_side_workers)
			_worker_free(worker);
		buf_free(self->sleepy_side_workers);

		for (auto worker : self->ready_side_workers)
			_worker_free(worker);
		buf_free(self->ready_side_workers);

		cond_var_free(self->cv);
		mutex_free(self->mtx);
		str_free(self->name);
		str_free(self->sysmon_name);
		task_free(self->settings.after_each_job);
		task_free(self->settings.on_worker_start);

		mutex_free(self->timer_system.mtx);
		for (auto& [_, timer]: self->timer_system.timers)
			task_free(timer->task);
		map_free(self->timer_system.timers);
		pool_free(self->timer_system.timers_pool);
		free(self);
	}

	void
	fabric_task_do(Fabric self, const Fabric_Task& task)
	{
		mutex_lock(self->mtx);
		mn_defer{mutex_unlock(self->mtx);};

		auto next_worker = self->next_worker++;
		next_worker %= self->workers.count;

		auto worker = self->workers[next_worker];
		worker_task_do(worker, task);

		self->atomic_available_jobs.fetch_add(1);
		cond_var_notify(self->cv);
	}

	void
	fabric_task_batch_do(Fabric self, const Fabric_Task* ptr, size_t count)
	{
		mutex_lock(self->mtx);
		mn_defer{mutex_unlock(self->mtx);};

		size_t increment = count / self->workers.count;
		if (increment == 0)
			increment = count;
		size_t added = 0;
		while (added < count)
		{
			auto to_add = increment;
			if (added + to_add >= count)
				to_add = count - added;

			auto next_worker = self->next_worker++;
			next_worker %= self->workers.count;

			auto worker = self->workers[next_worker];
			worker_task_batch_do(worker, ptr + added, to_add);

			added += to_add;
		}

		self->atomic_available_jobs.fetch_add(count);
		cond_var_notify(self->cv);
	}

	Fabric
	fabric_local()
	{
		Fabric res = nullptr;
		if (auto w = worker_local())
			res = w->fabric;
		return res;
	}

	size_t
	fabric_workers_count(Fabric self)
	{
		return self->workers.count;
	}

	// channel stream
	void
	IChan_Stream::dispose()
	{
		if (this->atomic_arc.fetch_sub(1) == 1)
		{
			chan_stream_close(this);

			mutex_free(this->mtx);
			cond_var_free(this->read_cv);
			cond_var_free(this->write_cv);
			free(this);
		}
	}

	Result<size_t, IO_ERROR>
	IChan_Stream::read(Block data_out)
	{
		chan_stream_ref(this);
		mn_defer{chan_stream_unref(this);};

		mutex_lock(this->mtx);
		if (this->data_blob.size == 0)
		{
			if (chan_stream_closed(this))
			{
				mutex_unlock(this->mtx);
				return IO_ERROR_CLOSED;
			}

			cond_var_wait(this->read_cv, this->mtx, [this]{
				return this->data_blob.size > 0 || chan_stream_closed(this);
			});
		}

		auto read_size = data_out.size;
		if (this->data_blob.size < read_size)
			read_size = this->data_blob.size;

		::memcpy(data_out.ptr, this->data_blob.ptr, read_size);
		auto ptr = (char*)this->data_blob.ptr;
		ptr += read_size;
		this->data_blob.ptr = ptr;
		this->data_blob.size -= read_size;
		bool ready_to_write = this->data_blob.size == 0;

		mutex_unlock(this->mtx);

		if (ready_to_write)
			cond_var_notify(this->write_cv);

		return read_size;
	}

	Result<size_t, IO_ERROR>
	IChan_Stream::write(Block data_in)
	{
		chan_stream_ref(this);
		mn_defer{chan_stream_unref(this);};

		// wait until there's available space
		mutex_lock(this->mtx);
		if (this->data_blob.size > 0)
		{
			cond_var_wait(this->write_cv, this->mtx, [this] {
				return this->data_blob.size == 0 || chan_stream_closed(this);
			});
		}

		if (chan_stream_closed(this))
			return IO_ERROR_CLOSED;

		// get the data
		this->data_blob = data_in;

		// notify the read
		cond_var_notify(this->read_cv);
		cond_var_wait(this->write_cv, this->mtx, [this] {
			return this->data_blob.size == 0 || chan_stream_closed(this);
		});
		auto res = data_in.size - this->data_blob.size;
		mutex_unlock(this->mtx);

		return res;
	}

	Chan_Stream
	chan_stream_new()
	{
		auto self = alloc_construct<IChan_Stream>();
		self->mtx = mn_mutex_new_with_srcloc("chan stream mutex");
		self->read_cv = cond_var_new();
		self->write_cv = cond_var_new();
		self->atomic_arc = 1;
		return self;
	}

	void
	chan_stream_free(Chan_Stream self)
	{
		if (self == nullptr)
			return;
		chan_stream_unref(self);
	}

	Chan_Stream
	chan_stream_ref(Chan_Stream self)
	{
		if (self == nullptr)
			return nullptr;

		self->atomic_arc.fetch_add(1);
		return self;
	}

	void
	chan_stream_unref(Chan_Stream self)
	{
		if (self == nullptr)
			return;

		self->dispose();
	}

	void
	chan_stream_close(Chan_Stream self)
	{
		if (self == nullptr)
			return;

		mutex_lock(self->mtx);
		self->atomic_closed.store(true);
		mutex_unlock(self->mtx);
		cond_var_notify_all(self->read_cv);
		cond_var_notify_all(self->write_cv);
	}

	bool
	chan_stream_closed(Chan_Stream self)
	{
		if (self == nullptr)
			return true;

		return self->atomic_closed.load();
	}
}
