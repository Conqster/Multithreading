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

	mNumWorkerThread = thread_count;
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
			_t->Process();
			delete _t;
			t = nullptr;
		}
	}
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
		//wait no task, semaphore
		uint32_t avail_task = 0;
		{
			std::unique_lock<std::mutex> no_task_lock(mTaskQueueMutex);

			mTaskAvailable.wait(no_task_lock, [this, &avail_task]()
				{
					avail_task = mAvailableTaskCount.load(std::memory_order_relaxed);
					return avail_task > 0;
				});
		}

		//after lock release, quick notify just incase of delay
		SignalAvailableTask(avail_task);
		if (mMainThreadWaitingTask)
			/// task_count - 1, has another could have be notified
			SignalMainThread(avail_task - 1);


		Task* task = nullptr;
		task = mPendingTasks[Utils::WrapPowerof2(mThreadTaskHead[thread_worker_idx], mTopPendingTasks - 1)].exchange(nullptr);
		//null means no task at slot or other thread pickup 
		if (task)
		{
			task->Process();
			//might have to delete heap allocated 
			delete task;
			//quick consume work counter and was successful 
			///but the means that the notify needs to be accurate 
			mAvailableTaskCount.fetch_sub(1, std::memory_order_acq_rel);
			THREAD_LOG_MSG(mThreadLogMutex, "____    Thread " << thread_worker_idx << " complete a Task. _____ Task Active: " << mProcessingTasks.load(std::memory_order_relaxed) << ".\n");
		}
		mThreadTaskHead[thread_worker_idx]++; //progress
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
	uint32_t pending_task_head = ComputeMinThreadHead();
	
	for (;;)
	{
		uint32_t pending_top = mTopPendingTasks.load();
		if (pending_task_head == mTopPendingTasks.load(std::memory_order_relaxed))
		{
			///if at the end 
			///wait and try again, might when to start at zero
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			pending_task_head = ComputeMinThreadHead();
		}

		Task* slot_expect = nullptr;
		bool succuss_add = mPendingTasks[pending_task_head].compare_exchange_strong(slot_expect, task);

		//new top 
		mTopPendingTasks.compare_exchange_strong(pending_top, pending_top + 1);//top is still top and then increament

		if (succuss_add)
		{
			break;
		}

		//move to the next if CAS failed 
		pending_task_head++;



		uint32_t pending_top = mTopPendingTasks.load();
		if(pending_task_head >= pending_top)
		{
			mTopPendingTasks.compare_exchange_strong(pending_top, pending_top + 1);//top is still top and then increament

			///we have reacg the end of already allocated 
			if (pending_task_head >= kMaxTaskQueue)
			{
				///there are 2 options here: 
				/// 1-> poke threads to do some work 
				///		so to have some space to right 
				///		has buffer/queue is full. 
				///		i.e signal, wait few microseconds then continue
				/// 
				/// 2-> reset head, put we are using wrap so this is not an issue 
				///		it would reset naturally. 
				///		meaning if max is 5 and the pending task head is >5 i.e 6 
				///		it would cause thois branch to be hit and stuck 
				///		so the hope is that thread head should be adjusted to wrap value 
				///		
				///if at the end 
				///wait and try again, might when to start at zero
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				pending_task_head = ComputeMinThreadHead();
				continue;
			}
		}


		Task* slot_expect = nullptr;
		bool succuss_add = mPendingTasks[Utils::WrapPowerof2(pending_task_head, kMaxTaskQueue - 1)].compare_exchange_strong(slot_expect, task);

		//new top 
		//mTopPendingTasks.compare_exchange_strong(pending_top, pending_top + 1);//top is still top and then increament

		if (succuss_add)
		{
			mAvailableTaskCount.fetch_add(1);
			break;
		}

		//move to the next if CAS failed 
		pending_task_head++;
	}

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

void TaskCoordinatorThreads::EnqueueTasks(Task** tasks, uint32_t count)
{

	size_t task_count = 0;
#if LOCKFREE_CAS_QUEUE
	for (Task** task = tasks, **task_end = tasks + count;
		task < task_end; ++task)
	{
		EnqueueTask(*task);
	}
	//each enqueue notifies
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
					return avail_task > 0 || mProcessingTasks.load(std::memory_order_relaxed) < 0;
				});

			if (avail_task < 0 && mProcessingTasks.load(std::memory_order_relaxed) <= 0) //check the job job quueue just incase of race condition 
				break;
		}

		//after lock release, quick notify just incase of delay
		SignalAvailableTask(avail_task);
		if (mMainThreadWaitingTask)
			/// task_count - 1, has another could have be notified
			SignalMainThread(avail_task - 1);


		Task* task = nullptr;
		task = mPendingTasks[Utils::WrapPowerof2(mThreadTaskHead[mNumWorkerThread], mTopPendingTasks - 1)].exchange(nullptr);
		//null means no task at slot or other thread pickup 
		if (task)
		{
			task->Process();
			//might have to delete heap allocated 
			delete task;
			//quick consume work counter and was successful 
			///but the means that the notify needs to be accurate 
			mAvailableTaskCount.fetch_sub(1, std::memory_order_acq_rel);
			THREAD_LOG_MSG(mThreadLogMutex, "____    MainThread complete a task._____\n");
		}
		mThreadTaskHead[mNumWorkerThread]++; //progress
	}
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