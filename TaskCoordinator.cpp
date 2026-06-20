#include "TaskCoordinator.h"


void Task::RemoveDependency()
{
	if (DecrementDependency())
		mCoordinator->EnqueueTask(this);
}


void TaskCoordinatorThreads::BeginThreads(int thread_count)
{
	if (thread_count < 0)
		thread_count = std::thread::hardware_concurrency() - 1;

	mOnQuitThreads = false;

	ASSERT(thread_count < kMaxThreads);
	ASSERT(mWorkers.empty());

#if LOCKFREE_CAS_QUEUE
	for (auto& t : mPendingTasks)
		t = nullptr;

	mPendingTasksTail = 0;
	mAvailableTaskCount = 0;

	mNumWorkerThread = thread_count;
	mNumActiveWorkerThread = thread_count;

	/// ensure mThreadTaskHead data is int/ trivial 
	//std::fill(mThreadTaskHead, &mThreadTaskHead[kMaxThreads + 1], 0);
	std::memset(mThreadTaskHead, 0, sizeof(mThreadTaskHead));
#endif LOCKFREE_CAS_QUEUE



	mWorkers.reserve(thread_count);
	for (int i = 0; i < thread_count; ++i)
		mWorkers.emplace_back([this, i] {ThreadsMainLoop(i); });
}

void TaskCoordinatorThreads::EndThreads()
{
	///* done
		//flag for program completion, which is shared by all threads. 
		//That keeps them spinning/updating and operation their would stop when program is done*/
		////sleep = true;
	for (auto& worker : mWorkers)
	{
		if (worker.joinable())
			worker.join();
	}

	///Reset Workers / Clear
	mWorkers.clear();

	/// any left jobs 
#if LOCKFREE_CAS_QUEUE
	if (mAvailableTaskCount.load() > 0)
	{
		for (auto& t : mPendingTasks)
		{
			Task* _t = t.load();
			if (_t == nullptr) continue;
			_t->Process();
			delete _t;
			t = nullptr;
		}
	}

	mPendingTasksTail = 0;
	mAvailableTaskCount = 0;
	mNumWorkerThread = 0;
	mNumActiveWorkerThread = 0;

	std::memset(mThreadTaskHead, 0, sizeof(mThreadTaskHead));
#else
	for (auto& task : mTasks)
	{
		task->Process();
		delete task;
		task = nullptr;
	}

	mTasks.clear();
#endif // LOCKFREE_CAS_QUEUE
}

#include <windows.h>
void SetThreadName(const char* threadName)
{
	const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
#pragma pack(pop)

	// DWORD dwThreadID = ::GetThreadId( static_cast<HANDLE>( t.native_handle() ) );
	//DWORD dwThreadID = ::GetThreadId(); 
	DWORD dwThreadID = (DWORD) - 1;

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try
	{
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

#include <string>
void TaskCoordinatorThreads::ThreadsMainLoop(int thread_worker_idx)
{
	THREAD_LOG_MSG(mThreadLogMutex, "About to commence worker thread idx: " << thread_worker_idx << " my os id " << std::this_thread::get_id() << "\n");

	{
		std::string _name = "Worker Thread ";
		_name += std::to_string(thread_worker_idx);
		SetThreadName(_name.c_str());
	}


#if LOCKFREE_CAS_QUEUE
	while (!mOnQuitThreads)
	{
		///keep scanning insytead of going back to sleep on fail
		//wait no task, semaphore
		uint32_t avail_task = 0;
		{
			std::unique_lock<std::mutex> no_task_lock(mTaskQueueMutex);

			mTaskAvailable.wait(no_task_lock, [this, &avail_task]()
				{
					avail_task = mAvailableTaskCount.load(std::memory_order_relaxed);
					return avail_task > 0 || mOnQuitThreads;
				});


			if (mOnQuitThreads)
				break;
		}


		///need to fix if awake, and there is task and not at tail 
		/// instead of sleeping, scan till tail before sleep
		while(mThreadTaskHead[thread_worker_idx] != mPendingTasksTail.load())
		{
			/// after lock release, quick notify just incase of delay
			/// notify - 1; because might get the task 
			/// but its not guaranteed
			SignalAvailableTask(avail_task - 1);
			if (mMainThreadWaitingTask)
				SignalMainThread(avail_task - 2); /// task_count - 1, has another could have be notified


			Task* task = nullptr;
			task = mPendingTasks[Utils::WrapPowerof2(mThreadTaskHead[thread_worker_idx].load(), kMaxTaskQueue - 1)].exchange(nullptr);

			//null means no task at slot or other thread pickup 
			if (task)
			{
				/// if we actual get the task 
				/// quick consume work counter and was successful 
				///but the means that the notify needs to be accurate 
				mAvailableTaskCount.fetch_sub(1, std::memory_order_acq_rel);

				/// we will be processing the so the waiting thread for processing task will be aware
				mProcessingTasks.fetch_add(1, std::memory_order_relaxed);

				task->Process();

				//we are done processing 
				mProcessingTasks.fetch_sub(1, std::memory_order_relaxed);

				/// signal main thread if waiting for task process complete 
				if (mMainThreadWaitingTask)
					SignalMainThread(1);

				//might have to delete heap allocated 
				delete task;
				THREAD_LOG_MSG(mThreadLogMutex, "____    Thread " << thread_worker_idx << " complete a Task. _____ Task Active: " << mProcessingTasks.load(std::memory_order_relaxed) << ".\n");
			}
			mThreadTaskHead[thread_worker_idx]++; //progress

			SignalMainThread(1);

			//check reflection to prevent, unnecessary scanning 
			avail_task = mAvailableTaskCount.load(std::memory_order_relaxed);
			if (avail_task <= 0)
				break;

		}

	}
#else

	while (!mOnQuitThreads)
	{
		_TaskBase* task = nullptr;
		size_t task_count = 0;
		{
			std::unique_lock<std::mutex> queue_mutex(mTaskQueueMutex);

			const auto& tasks = &mTasks;
			mTaskAvailable.wait(queue_mutex, [this, tasks]() {
				return !tasks->empty() || mOnQuitThreads;
				//return !mJobs.empty() || mOnQuitThreads;
				});

			if (mOnQuitThreads && mTasks.empty())
				break;


			task = mTasks.front();
			//remove job 
			mTasks.pop_front();
			//mActiveJobs++; //lock freee atomic increment
			mProcessingTasks.fetch_add(1, std::memory_order_relaxed);

			task_count = mTasks.size();
		}

		//if (task_count > 0)
		//	mTaskAvailable.notify_one();
		SignalAvailableTask(task_count);

		//execute job
		task->Process();

		mProcessingTasks.fetch_sub(1, std::memory_order_relaxed);

		if (mMainThreadWaitingTask)
			/// task_count - 1, has another could have be notified
			SignalMainThread(task_count);

		delete task;
		task = nullptr;


		THREAD_LOG_MSG(mThreadLogMutex, "____    Thread " << thread_worker_idx << " complete a Task. _____ Task Active: " << mProcessingTasks.load(std::memory_order_relaxed) << ".\n");
	}

#endif // LOCKFREE_CAS_QUEUE
	THREAD_LOG_MSG(mThreadLogMutex, "____    Thread " << thread_worker_idx << " done. _____\n");
}

void TaskCoordinatorThreads::EnqueueTask(Task* task)
{
	size_t task_count = 0;

#if LOCKFREE_CAS_QUEUE
	/// use the far head (min) and tail, to capture immediate task range
	/// 
	/// bitwise wrap is used
	/// keep adjusting pending tail, until success 
	/// 
	/// but if the range btw far back head (min) and tail greater then pendable task
	/// 
	/// recompute far head as worker should have completed some task which might have 
	/// reduce the range, so to continue adjusting the tail. 
	/// 
	/// but after recomputing, it must have mean that the buffer is completely full.
	///  poke all worker thread , then go to quick few microsecond sleep to retry
	/// 
	///
	uint32_t capture_min_head = ComputeMinThreadHead();
	uint32_t dummy_wake = 0;
	for (;;)
	{
		uint32_t curr_task_tail = mPendingTasksTail;
		//far back head (min) and tail range
		if (curr_task_tail - capture_min_head >= kMaxTaskQueue)
		{
			//recapute far back head min
			capture_min_head = ComputeMinThreadHead();
			/// is the range still greater, wait for worker thread
			/// to catch up
			if (curr_task_tail - capture_min_head >= kMaxTaskQueue)
			{
				///hack as semaphore to wake 
				mAvailableTaskCount.fetch_add(1, std::memory_order_relaxed);
				dummy_wake++;
				PokeWorkers();
				//THREAD_LOG_MSG("Waiting to add more tasks, poked workers \n");
				//{
				//	mThreadLogMutex.lock(); \
				//	LOG_MSG("Waiting to add more task, poked workers \n"); \
				//	mThreadLogMutex.unlock();
				//}
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				continue;
			}
		}

		Task* t_expect = nullptr;
		bool succuss_add = mPendingTasks[Utils::WrapPowerof2(curr_task_tail, kMaxTaskQueue - 1)].compare_exchange_strong(t_expect, task);

		/// move to the next if CAS failed
		/// 
		mPendingTasksTail.compare_exchange_strong(curr_task_tail, curr_task_tail + 1);

		/// new tail 
		/// task was add to curr end of pending task
		if (succuss_add)
		{
			//for debug
			mAvailableTaskCount.fetch_add(1);
			break;
		}
	}
	//reload task available for trigging
	task_count = mAvailableTaskCount.load(std::memory_order_relaxed);
	

#else
	{
		std::lock_guard<std::mutex> queue_mutex(mTaskQueueMutex);
		mTasks.push_back(task);
		task_count = mTasks.size();
	}

#endif // LOCKFREE_CAS_QUEUE

	//signal/notify threads waiting on flag
	SignalAvailableTask(task_count);
	if (mMainThreadWaitingTask)
		SignalMainThread(task_count - 1);/// task_count - 1, has another could have be notified
}

void TaskCoordinatorThreads::EnqueueTasks(Task** tasks, const uint32_t count)
{
	size_t task_count = 0;
#if LOCKFREE_CAS_QUEUE
	//for (Task** task = tasks, **task_end = tasks + count;
	//	task < task_end; ++task)
	//	EnqueueTask(*task);

	////each enqueue notifies

	/// use the far head (min) and tail, to capture immediate task range
	/// 
	/// bitwise wrap is used
	/// keep adjusting pending tail, until success 
	/// 
	/// but if the range btw far back head (min) and tail greater then pendable task
	/// 
	/// recompute far head as worker should have completed some task which might have 
	/// reduce the range, so to continue adjusting the tail. 
	/// 
	/// but after recomputing, it must have mean that the buffer is completely full.
	///  poke all worker thread , then go to quick few microsecond sleep to retry
	/// 
	

	//Task** curr_task = tasks;
	//Task** task_end = tasks + count;

	uint32_t queued_pointer = 0;
	uint32_t dummy_wake = 0;
	uint32_t capture_min_head = ComputeMinThreadHead();
	for (;;)
	{
		uint32_t curr_task_tail = mPendingTasksTail;
		//far back head (min) and tail range
		if (curr_task_tail - capture_min_head >= kMaxTaskQueue)
		{
			//recapute far back head min
			capture_min_head = ComputeMinThreadHead();
			/// is the range still greater, wait for worker thread
			/// to catch up
			if (curr_task_tail - capture_min_head >= kMaxTaskQueue)
			{
				///hack as semaphore to wake 
				mAvailableTaskCount.fetch_add(1, std::memory_order_relaxed);
				dummy_wake++;
				PokeWorkers();
				////THREAD_LOG_MSG("Waiting to add a task, poked workers \n");
				//{
				//	mThreadLogMutex.lock(); \
				//	LOG_MSG("Waiting to add a task, poked workers \n"); \
				//	mThreadLogMutex.unlock();
				//}
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				continue;
			}
		}

		Task* t_expect = nullptr;
		//bool succuss_add = mPendingTasks[Utils::WrapPowerof2(curr_task_tail, kMaxTaskQueue - 1)].compare_exchange_strong(t_expect, *curr_task);
		bool succuss_add = mPendingTasks[Utils::WrapPowerof2(curr_task_tail, kMaxTaskQueue - 1)].compare_exchange_strong(t_expect, tasks[queued_pointer]);

		/// move to the next if CAS failed
		/// 
		mPendingTasksTail.compare_exchange_strong(curr_task_tail, curr_task_tail + 1);

		/// new tail 
		/// task was add to curr end of pending task
		if (succuss_add)
		{
			//for debug
			mAvailableTaskCount.fetch_add(1);

			queued_pointer++;
			if (queued_pointer >= count)
				break;
			//curr_task++;
			//if(curr_task >= task_end)
			//	break;
		}
	}
	//reload task available for trigging
	task_count = mAvailableTaskCount.load(std::memory_order_relaxed);
	
	SignalAvailableTask(task_count);
	if (mMainThreadWaitingTask)
		SignalMainThread(task_count - 1);/// task_count - 1, has another could have be notified
#else
	{
		std::lock_guard<std::mutex> queue_mutex(mTaskQueueMutex);

		for (Task** task = tasks, **task_end = tasks + count;
			task < task_end; ++task)
		{
			mTasks.push_back(*task);
		}

		task_count = mTasks.size();
	}

	//signal/notify threads waiting on flag
//mJobFlag.notify_one();
	SignalAvailableTask(task_count);
	if (mMainThreadWaitingTask)
		SignalMainThread(task_count - 1);/// task_count - 1, has another could have be notified
#endif // LOCKFREE_CAS_QUEUE
}

void TaskCoordinatorThreads::SignalMainThread(size_t task_count)
{
	if (mProcessingTasks.load(std::memory_order_relaxed) <= 0 || task_count > 0)
	{
		/// if main thread is waiting for active jobs 
		mMainWaitFlag.notify_one();
	}
}

void TaskCoordinatorThreads::WaitForTasks()
{
	THREAD_LOG_MSG(mThreadLogMutex, "Wating for tasks completion.......!\n");



	mMainThreadWaitingTask = true;
	//for helping out 
	_TaskBase* task;

	///later while waiting for task 
	///also support quit break as well
#if LOCKFREE_CAS_QUEUE

	/// for hack catch up 
	mThreadTaskHead[mWorkers.size()] = ComputeMinThreadHead();
	mNumActiveWorkerThread = mNumWorkerThread + 1;

	while (!mOnQuitThreads)
	{
		THREAD_LOG_MSG(mThreadLogMutex, "____  Main Thread is waiting for " << mAvailableTaskCount.load(std::memory_order_relaxed) << " tasks and " << mProcessingTasks << " active tasks to complete_____\n");
		//wait no task, semaphore
		uint32_t avail_task = 0;
		{
			std::unique_lock<std::mutex> no_task_lock(mTaskQueueMutex);

			mMainWaitFlag.wait(no_task_lock, [this, &avail_task]()
				{
					avail_task = mAvailableTaskCount.load(std::memory_order_relaxed);
					return avail_task > 0 || mProcessingTasks.load(std::memory_order_relaxed) <= 0;
				});

			if (avail_task <= 0 && mProcessingTasks.load(std::memory_order_relaxed) <= 0) //check the job job quueue just incase of race condition 
				break;
		}

		///// after lock release, quick notify just incase of delay
		///// notify - 1; because might get the task 
		///// but its not guaranteed
		//SignalAvailableTask(avail_task - 1);
		//if (mMainThreadWaitingTask)
		//	SignalMainThread(avail_task - 2); /// task_count - 1, has another could have be notified


		//Task* task = nullptr;
		//task = mPendingTasks[Utils::WrapPowerof2(mThreadTaskHead[mNumWorkerThread].load(std::memory_order_relaxed), kMaxTaskQueue - 1)].exchange(nullptr);
		////null means no task at slot or other thread pickup 
		//if (task)
		//{
		//	/// if we actual get the task 
		//	/// quick consume work counter and was successful 
		//	///but the means that the notify needs to be accurate 
		//	mAvailableTaskCount.fetch_sub(1, std::memory_order_acq_rel);

		//	/// we will be processing the so the waiting thread for processing task will be aware
		//	mProcessingTasks.fetch_add(1, std::memory_order_relaxed);

		//	task->Process();

		//	//we are done processing 
		//	mProcessingTasks.fetch_sub(1, std::memory_order_relaxed);

		//	//might have to delete heap allocated 
		//	delete task;
		//	THREAD_LOG_MSG(mThreadLogMutex, "____    MainThread complete a task._____\n");
		//}
		//mThreadTaskHead[mNumWorkerThread]++; //progress



		///need to fix if awake, and there is task and not at tail 
		/// instead of sleeping, scan till tail before sleep
		while (mThreadTaskHead[mNumWorkerThread] != mPendingTasksTail.load())
		{
			/// after lock release, quick notify just incase of delay
			/// notify - 1; because might get the task 
			/// but its not guaranteed
			SignalAvailableTask(avail_task - 1);
			if (mMainThreadWaitingTask)
				SignalMainThread(avail_task - 2); /// task_count - 1, has another could have be notified


			Task* task = nullptr;
			task = mPendingTasks[Utils::WrapPowerof2(mThreadTaskHead[mNumWorkerThread].load(), kMaxTaskQueue - 1)].exchange(nullptr);

			//null means no task at slot or other thread pickup 
			if (task)
			{
				/// if we actual get the task 
				/// quick consume work counter and was successful 
				///but the means that the notify needs to be accurate 
				mAvailableTaskCount.fetch_sub(1, std::memory_order_acq_rel);

				/// we will be processing the so the waiting thread for processing task will be aware
				mProcessingTasks.fetch_add(1, std::memory_order_relaxed);

				task->Process();

				//we are done processing 
				mProcessingTasks.fetch_sub(1, std::memory_order_relaxed);

				//might have to delete heap allocated 
				delete task;
				THREAD_LOG_MSG(mThreadLogMutex, "____    MainThread complete a task._____\n");
			}
			mThreadTaskHead[mNumWorkerThread]++; //progress

			//check reflection to prevent, unnecessary scanning 
			avail_task = mAvailableTaskCount.load(std::memory_order_relaxed);
			if (avail_task <= 0)
				break;
		}

		if (mProcessingTasks.load(std::memory_order_relaxed) <= 0)
			break;
	}

	mNumActiveWorkerThread = mNumWorkerThread;

#else
	//wait to .2 of a sec for a signal. 
	//std::unique_lock<std::mutex> queue_mutex(m_JobPool.m_PoolMutex);
	//std::unique_lock<std::mutex> queue_mutex(mJobQueueMutex);
	//std::memory_order
	while (!mTasks.empty() || mProcessingTasks.load(std::memory_order_acquire) > 0)
	{
		THREAD_LOG_MSG(mThreadLogMutex, "____  Main Thread is waiting for " << mTasks.size() << " tasks and " << mProcessingTasks << " active tasks to complete_____\n");
		//std::this_thread::sleep_for(std::chrono::microseconds(5));

		size_t task_count = 0;
		
		///help out 
		{
			std::unique_lock<std::mutex> queue_mutex(mTaskQueueMutex); 
			////this is bad main thread does not release mutex lock, when job and other thread are processing

			mMainWaitFlag.wait(queue_mutex, [this]() {
				return !mTasks.empty() || (mProcessingTasks.load(std::memory_order_relaxed) <= 0);
				});

			if (mTasks.empty() && mProcessingTasks.load(std::memory_order_relaxed) <= 0) //check the job job quueue just incase of race condition 
				break;

			//excute jobs 
			if (!mTasks.empty()) //check the job job quueue just incase of race condition 
			{
				task = mTasks.front();
				//remove job 
				mTasks.pop_front();
				//mActiveJobs++; //lock freee atomic increment
				mProcessingTasks.fetch_add(1, std::memory_order_relaxed);

				task_count = mTasks.size();
			}
			else
			{
				//if another thread as pick up work before this 
				task = nullptr;
				//maybe later have a cascade wrap for multiple cv
				//sleep if no job, but is new job (rare) or active job finised
				//mMainWaitFlag.wait(queue_mutex, [this]() {
				//	return !mTasks.empty() || (mProcessingTasks.load(std::memory_order_relaxed) <= 0);
				//	});

				//wrap check
				continue;
			}
		}
		///unique_lock mutex lock scope

		//if (task_count > 0)
		//	mTaskAvailable.notify_one();
		SignalAvailableTask(task_count);


		if (task)
		{
			//mThreadLogMutex.lock();
			//job();
			task->Process();
			//later ref count smart pointeer
			delete task;
			task = nullptr;
			//mThreadLogMutex.unlock();
			mProcessingTasks.fetch_sub(1, std::memory_order_relaxed);

			THREAD_LOG_MSG(mThreadLogMutex, "____    MainThread complete a task._____\n");
		}

	}
#endif LOCKFREE_CAS_QUEUE
	mMainThreadWaitingTask = false;
	THREAD_LOG_MSG(mThreadLogMutex, "Thread Work finished !!!!!!!\n");
}