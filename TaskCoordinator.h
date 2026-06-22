#pragma once

#include "Core.h"

#include <thread>

#include <queue>

#include <functional>
#include <mutex>

#define DEBUG_THREAD 0
#define DISABLE_THREAD_LOG 1

#if DISABLE_THREAD_LOG
#define THREAD_LOG_MSG(mutex, msg) 
#else
#define THREAD_LOG_MSG(mutex, msg) \
	mutex.lock(); \
	LOG_MSG(msg); \
	mutex.unlock(); 
#endif // DISABLE_THREAD_LOG


//
//
////so to have per/local thread parallel 
//std::atomic<int> subTaskCounter = 100;
//void ParallelForSubTask(void* task, std::atomic<int> subTaskCounter)
//{
//
//}
//void ThreadWaitSubTaskCounter(std::atomic<int> subTaskCounter)
//{
//	while (subTaskCounter.load(std::memory_order_relaxed) > 0)
//	{
//		//process sub task
//	}
//}
//

#define LOCKFREE_CAS_QUEUE 1
#if LOCKFREE_CAS_QUEUE
#include <array>
#endif // LOCKFREE_CAS_QUEUE



class TaskCoordinator;
class TaskCoordinatorSequential;
class TaskCoordinatorThreads;

using TaskFunction = std::function<void()>;


class Task 
{
public:
	Task() = delete;
	Task(TaskCoordinator* coordinator, const TaskFunction& _func, uint32_t _dependancies) :
		mFunc(_func), mCoordinator(coordinator), mDependencies(_dependancies) {}

	void IncrementDependency() { mDependencies.fetch_add(1, std::memory_order_relaxed);}
	void RemoveDependency();

	bool Process()
	{
		if (mDependencies.load(std::memory_order_relaxed) > 0)
			return false;
		mFunc();
		//successfully complete job
		return true;
	}
private:
	friend TaskCoordinator;
	friend TaskCoordinatorThreads;
	friend TaskCoordinatorSequential;
	bool DecrementDependency()
	{
		uint32_t v = mDependencies.fetch_sub(1, std::memory_order_relaxed);
		uint32_t _v = v - 1;
		ASSERT(v > _v);
		return _v == 0;
	}

	TaskFunction mFunc;
	TaskCoordinator* mCoordinator = nullptr;
	std::atomic<uint32_t> mDependencies = 0;
};


class TaskCoordinator : public NonCopyable
{
public:
	TaskCoordinator() = default;
	virtual ~TaskCoordinator() = default;

	//class Semaphore
	//{
	//public:
	//	void Signal(uint32_t count)
	//	{
	//		ASSERT(count > 0);
	//		mMutex.lock();
	//		mCount.fetch_add(count, std::memory_order_relaxed);
	//		mMutex.unlock();
	//		//if (count > 1)
	//		if (mCount.load(std::memory_order_relaxed) > 1)
	//			mCounter.notify_one();
	//		else
	//			mCounter.notify_all();
	//	}
	//	void Acquire(uint32_t require_count)
	//	{
	//		std::unique_lock<std::mutex> mutex_lock(mMutex);
	//		mCounter.wait(mutex_lock, [this, require_count]() {
	//			return mCount.load(std::memory_order_relaxed) >= require_count;
	//			});
	//		mCount.fetch_sub(require_count, std::memory_order_relaxed);
	//	}
	//private:
	//	std::condition_variable mCounter;
	//	std::atomic<uint32_t> mCount;
	//	std::mutex mMutex;
	//};
	//void Test()
	//{
	//	Semaphore mThreadJobSemaphore;
	//	///new a job
	//	mThreadJobSemaphore.Signal(1);
	//	//later for batch
	//	mThreadJobSemaphore.Signal(5/*N*/);
	//	//in thread loop 
	//	mThreadJobSemaphore.Acquire(1);
	//	//quick hack arounf, on quit threads 
	//	mOnQuitThreads = true;
	//	mThreadJobSemaphore.Signal(mNumThread); //<-- which signals all thread
	//}

	virtual Task* ConstructTask(const TaskFunction& task_func, uint32_t dependencies) = 0;
	virtual void EnqueueTask(Task* task) = 0;
	virtual void EnqueueTasks(Task** tasks, uint32_t count) = 0;

	virtual void Quit() = 0;
	virtual void WaitForTasks() = 0;
};

class TaskCoordinatorSequential : public TaskCoordinator
{
public:
	TaskCoordinatorSequential() = default;
	~TaskCoordinatorSequential() = default;

	Task* ConstructTask(const TaskFunction& task_func, uint32_t dependencies) override
	{
		//ASSERT(dependencies == 0);
		Task* t = new Task(this, task_func, dependencies);
		if (dependencies == 0)
		{
			ProcessTask(task_func);
			delete t;
		}
		return t;
	}



	void EnqueueTask(Task* task) override { ProcessTask(task->mFunc); delete task; }
	void EnqueueTasks(Task** tasks, uint32_t count)
	{
		for (Task** task = tasks, **task_end = tasks + count;
			task < task_end; ++task)
		{
			Task* _task = *task;
			ProcessTask(_task->mFunc);
			delete _task;
		}
	}
	void Quit() override{}
	void WaitForTasks() override {}

	void RemoveTasksDependency(Task** tasks, uint32_t task_count)
	{
		// check if task is part of coordinator 
		/// const uint8* _addr = reinterpret_cast<const uint8*>(addr);
		/// return _addr >= mMemStart && _addr < mMemStart + mStackSize;
		/// 
		ASSERT(task_count > 0);

		Task** enqueuing_tasks = (Task**)alloca(task_count * sizeof(Task*));
		Task** enqueuing_top = enqueuing_tasks;

		for (Task** task = tasks, **task_end = tasks + task_count;
			task < task_end; ++task)
		{
			if ((*task)->DecrementDependency())
				*(enqueuing_top++) = *task;
		}

		uint32_t task_to_queue = uint32_t(enqueuing_top - enqueuing_tasks);
		if (task_to_queue > 0)
			EnqueueTasks(enqueuing_tasks, task_to_queue);
	}

private:
	__forceinline void ProcessTask(TaskFunction func) const { func(); }
};

class TaskCoordinatorThreads : public TaskCoordinator
{
public:

	//TaskCoordinatorThreads() = default;
	TaskCoordinatorThreads(int thread_count = -1) //: mNumThread(thread_count) 
	{
		BeginThreads(thread_count);
	}

	~TaskCoordinatorThreads() { EndThreads(); }

	Task* ConstructTask(const TaskFunction& task_func, uint32_t dependencies) override
	{
		Task* task = ConstructTaskInternal(task_func, dependencies);
		if(dependencies == 0)
			EnqueueTask(task);
		return task;
	}

	void EnqueueTask(Task* task) override;
	void EnqueueTasks(Task** tasks, uint32_t count) override;

	void RemoveTaskDependency(Task* task)
	{
		// check if task is part of coordinator 
		/// const uint8* _addr = reinterpret_cast<const uint8*>(addr);
		/// return _addr >= mMemStart && _addr < mMemStart + mStackSize;
		if (task->DecrementDependency())
		{
			//now we can add task
			EnqueueTask(task);
		}
	}

	/// behaves as a sub task, where a thread/task 
/// break works down to be process 
	void ParallelFor(TaskCoordinator* coord)
	{


	}

	void RemoveTasksDependency(Task** tasks, uint32_t task_count)
	{
		// check if task is part of coordinator 
		/// const uint8* _addr = reinterpret_cast<const uint8*>(addr);
		/// return _addr >= mMemStart && _addr < mMemStart + mStackSize;
		/// 
		ASSERT(task_count > 0);

		Task** enqueuing_tasks = (Task**)alloca(task_count * sizeof(Task*));
		Task** enqueuing_top = enqueuing_tasks;

		for (Task** task = tasks, **task_end = tasks + task_count;
			task < task_end; ++task)
		{
			if ((*task)->DecrementDependency())
				*(enqueuing_top++) = *task;
		}

		uint32_t task_to_queue = uint32_t(enqueuing_top - enqueuing_tasks);
		if (task_to_queue > 0)
			EnqueueTasks(enqueuing_tasks, task_to_queue);
	}


	void Quit() override
	{
		mOnQuitThreads = true;
		mTaskAvailable.notify_all(); //notify all using this flag on mOnQuitThreads
	}

	void QuitAndEndThreads()
	{
		Quit();
		EndThreads();
	}

	void WaitForTasks() override;
private:
	void ThreadsMainLoop(int thread_worker_idx);

	/// if thread count is 0 or less, 
	/// the coordinator use max hardward supported count
	void BeginThreads(int thread_count);
	/// Terminates threads
	void EndThreads();

	Task* ConstructTaskInternal(TaskFunction task_func, uint32_t dependencies)
	{
		//#define USE_TASK_BUFFER
		return new Task(this, task_func, dependencies);
	}

	__forceinline void SignalAvailableTask(size_t task_count)
	{
		//mTaskAvailable.notify_all();
		//return;
		if (task_count > 1)
			mTaskAvailable.notify_all();
		else
			mTaskAvailable.notify_one();
	}
	void SignalMainThread(size_t task_count);

	void PokeWorkers()
	{
		mTaskAvailable.notify_all();
		mMainWaitFlag.notify_one();
	}

	/// it should be easy to check if a task belong to 
	/// task coordinator. 
	/// by using it pointer address 
	/// hence 
	/// we have the begin of allo and end 
	/// as a linear allocator 
	/// if the address is within the allocation 
	/// region then this task belong to task coordinator 
private:
	//int mNumThread = 0;
	bool mOnQuitThreads = false;
	static constexpr int kMaxThreads = 32;
	std::vector<std::thread> mWorkers;
	std::mutex mThreadLogMutex;

	std::mutex mTaskQueueMutex;
	/// flag for available/pending tasks
	std::condition_variable mTaskAvailable;
	std::condition_variable mMainWaitFlag;




	/// pending tasks
#if LOCKFREE_CAS_QUEUE
	static constexpr uint32_t kMaxTaskQueue = 1024; //need to be power of 2 to support wrapping

	std::array<std::atomic<Task*>, kMaxTaskQueue> mPendingTasks;
	std::atomic<uint32_t> mPendingTasksTail = 0;
	std::atomic<uint32_t> mAvailableTaskCount = 0;

	/// mainly used to access the Main thread task head
	uint32_t mNumWorkerThread = 0;
	uint32_t mNumActiveWorkerThread = 0;

	std::atomic<uint32_t> mThreadTaskHead[kMaxThreads + 1]; //extra space to main thread, during task waiting
	static_assert(std::is_integral_v<std::remove_reference_t<decltype(mThreadTaskHead[0])>::value_type>);
	static_assert(sizeof(mThreadTaskHead[0]) == 4);

	/// min head, based on the firtst/small thread head
	/// to ensure task gravitates around thread scan window
	/// reduncing threads from wasting clock cyles scanning empty 
	/// mem region
	uint32_t ComputeMinThreadHead()
	{
		///just to check if main thread is participating
		//uint32_t workers = (mMainThreadWaitingTask) ? mWorkers.size() + 1 : mWorkers.size();
		uint32_t workers = mNumActiveWorkerThread;
		uint32_t temp = mThreadTaskHead[0];
		for (int i = 1; i < workers; ++i) //+1 as the main thread uses the slot to last work thread
			temp = std::min(temp, mThreadTaskHead[i].load());
		return temp;
	}
#else
	std::deque<Task*> mTasks;
#endif // LOCKFREE_CAS_QUEUE



	/// might need a mutex for task construct 
	/// or move the mTaskQueueMutex to be task construct 
	/// mutex, so one thread can access at a time, 
	/// 
	/// but then the queue, to use atomics then 
	/// later after learning more about 
	/// compare and exchange to make lock free
	static constexpr int kMaxTaskConstruct = 1024;
	using TaskConstructBuffer = std::array<Task, kMaxTaskConstruct>;
	//TaskConstructBuffer mTaskBuffer{};

	class TaskBuffer : public NonCopyable
	{
	private:
		struct BufferData
		{
			Task taskData; 
			/// so instead of have a list of free when have a chain of free
			/// buffer points to the first/head of chain
			/// so we would have to manage multiple datas
			std::atomic<uint32_t> nextFree;
		};
		static_assert(alignof(BufferData) == alignof(Task));
	public:
		void Init(size_t max_task)
		{
			mMax = max_task;
			//mBuffer = new BufferData[max_task]; //no default constructot 
			mBuffer = reinterpret_cast<BufferData*>(new uint8_t[sizeof(BufferData) * max_task]);
		}

		/// just in case
		template<typename ...Params>
		uint32_t Construct(Params&&... params)
		{

		}

		~TaskBuffer()
		{
			delete[] mBuffer;
		}
	private:
		BufferData* mBuffer;
		uint32_t mMax;
		std::atomic<uint32_t> mFirstNextFree;
	};

	bool mMainThreadWaitingTask = false;

	/// current task been processed by threads
	std::atomic<int> mProcessingTasks = 0;
};


