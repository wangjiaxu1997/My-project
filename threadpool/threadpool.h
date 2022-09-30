#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>

//不要出现魔鬼数字
const int TASK_MAX_THRESHHOLD = 2;//INT32_MAX; // 任务数量上限阈值
const int THREAD_MAX_THRESHHOLD = 10;	// 线程数量上限阈值
const int THREAD_MAX_IDLE_TIME = 60;	// 单位：秒

// 线程池支持类型
enum class PoolMode
{
	MODE_FIXED,	// 固定线程数量
	MODE_CACHED, // 可动态增长线程数量
};

// 线程类型
class Thread
{
public:
	// 线程函数对象类型
	using ThreadFun = std::function<void(int)>;

	// 构造
	Thread(ThreadFun func)
		: func_(func)
		, threadId_(generateId_++)
	{}
	// 析造
	~Thread() = default;

	// 线程启动
	void start()
	{
		//创建一个线程来执行线程行数
		std::thread t(func_, threadId_); //C++来说 线程对象t 线程函数func_
		t.detach(); //设置分离线程
	}

	// 获取线程Id
	int getId() const
	{
		return threadId_;
	}

private:
	ThreadFun func_;

	static int generateId_;
	int threadId_;	// 保存线程Id
};
int Thread::generateId_ = 0;

// 线程池
class ThreadPool
{
public:
	ThreadPool()
		:initThreadSize_(0),
		taskSize_(0),
		idleThreadSize_(0),
		curThreadSize_(0),
		taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
		threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
		poolMode_(PoolMode::MODE_FIXED),
		isPoolRuning_(false)
	{}

	~ThreadPool()
	{
		isPoolRuning_ = false;
		//notEmpty_.notify_all();

		// 等待线程池的所有线程池返回
		std::unique_lock<std::mutex> myLock(taskQueMtx);
		notEmpty_.notify_all();

		exitCond_.wait(myLock, [&]()->bool {return threads_.size() == 0; });
	}

	// 设置线程池的工作模式
	void setMode(PoolMode mode)
	{
		if (checkRuningState())
			return;

		poolMode_ = mode;
	}

	//设置初始线程数量
	void setInitThreadSixe(int size);

	// 设置任务队列数量上限阈值
	void setTaskQueMaxThreshHol(int threshHold_)
	{
		if (checkRuningState())
			return;

		if (poolMode_ == PoolMode::MODE_CACHED)
			taskQueMaxThreshHold_ = threshHold_;
	}
	// 设置线程数量上限阈值
	void setThreadSizeThreshHol(int threshHold_)
	{
		if (checkRuningState())
			return;

		threadSizeThreshHold_ = threshHold_;
	}

	// 给线程提交任务
	// 使用可变参模板编程，让submitTask可以接收任意函数和任意数量参数
	// Result submitTask(std::shared_ptr<Task> sp);
	// 返回值future<返回值类型>
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		//打包任务，放到任务队列里面
		using RType = decltype(func(args...));
		auto task = std::make_shared < std::packaged_task<RType() >>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

		// 获取锁
		std::unique_lock<std::mutex> myMtx(taskQueMtx);

		if (!notFull_.wait_for(myMtx, std::chrono::seconds(1),
			[&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; }))
		{
			// 等待一秒钟条件仍然没有满足
			std::cerr << "task queue is full submitTask fail!" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType { return RType(); });
			(*task)();
			return task->get_future();
		}

		// 如果有空余 把任务放到任务队列中
		//taskQue_.emplace(sp);
		//using Task = std::function<void()>;
		taskQue_.emplace([task]() {(*task)(); });
		taskSize_++;

		// 把任务放到任务队列中后 notEmpty_通知线程池分配线程执行任务
		notEmpty_.notify_all();

		// cached模式 根据任务数量和空闲线程数量，判断是否需要创建新的线程
		//任务小而快
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ < threadSizeThreshHold_)
		{
			//创建新线程
			std::cout << ">>>>>>>create new thread!" << std::endl;
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			//启动线程
			threads_[threadId]->start();
			//修改线程相关变量
			curThreadSize_++;
			idleThreadSize_++;
		}

		return result;
	}
	// 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency()/*返回当前系统的CPU核心数*/)
	{
		//设置线程运行转态
		isPoolRuning_ = true;

		// 记录初始线程数量
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;

		// 创建线程对象
		for (int i = 0; i < initThreadSize_; i++)
		{
			// 创建Thread线程对象的时候，把线程函数给到thread对象
			// std::unique_ptr<Thread>
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			// threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			// threads_.emplace_back(std::move(ptr));
		}

		// 启动所有线程 std::vector<Thread*> threads_;
		for (int i = 0; i < initThreadSize_; i++)
		{
			threads_[i]->start(); // 需要去执行一个线程函数
			idleThreadSize_++;	//记录初始空闲线程数量
		}
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//定义线程函数
	void threadFunc(int threadId)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();

		for (;;)
			//while(isPoolRuning_)
		{
			//std::shared_ptr<Task> task;
			Task task;
			{
				// 先获取锁
				std::unique_lock<std::mutex> myMtx(taskQueMtx);
				std::cout << "tid= " << std::this_thread::get_id() << "尝试获取任务..... " << std::endl;

				//每秒返回1次,怎么区分：超时返回  还是有任务待执行返回
				while (/*isPoolRuning_ && */taskQue_.size() == 0) //锁 + 双重判断
				{
					if (!isPoolRuning_)
					{
						//线程池要结束回收资源
						//线程池执行完任务要结束回收资源
						threads_.erase(threadId);	// 结束所有线程
						std::cout << "threadId:" << std::this_thread::get_id() << "exit!" << std::endl;
						exitCond_.notify_all();
						return;
					}

					//cached模式下 把空闲时间超过60s的空闲线程（超过initThreadSize_数量的线程）回收
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						// 条件变量 超时返回
						if (std::cv_status::timeout ==
							notEmpty_.wait_for(myMtx, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
							{
								// 开始回收当前线程
								// 记录线程相关变量的值修改
								// 把线程对象从线程列表容器中删除
								// threadId->thread对象->删除
								threads_.erase(threadId);	// std::this_thread::get_id()
								std::cout << "threadId:" << std::this_thread::get_id() << "exit!"
									<< std::endl;

								curThreadSize_--;
								idleThreadSize_--;
								return;
							}
						}
					}
					else
					{
						// 等待notEmpty条件
						notEmpty_.wait(myMtx, [&]()->bool { return taskQue_.size() > 0; });
					}
				}
				idleThreadSize_--;	//	线程取任务执行

				std::cout << "tid= " << std::this_thread::get_id() << "获取任务成功..... " << std::endl;

				// 从任务队列中取一个任务出来
				task = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				// 如果有剩余任务，通知其他线程执行任务
				if (taskQue_.size() > 0)
				{
					notEmpty_.notify_all();
				}

				// 取出一个任务，进行通知 可以继续提交任务
				notFull_.notify_all();

			}   // 出作用域就可以把锁释放掉

			// 当前线程负责执行这个任务
			if (task != nullptr)
			{
				//task->run();
				//task->exec();
				task(); // 执行function<void()>
			}

			idleThreadSize_++; //执行完任务 转为空闲线程
			auto lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
		}

		////线程池执行完任务要结束回收资源
		//threads_.erase(threadId);	// 结束所有线程
		//std::cout << "threadId:" << std::this_thread::get_id() << "exist!" << std::endl;
		//exitCond_.notify_all();
		//return;
	}

	//检查pool的运行状态
	bool checkRuningState() const
	{
		return isPoolRuning_;
	}

private:
	//std::vector<std::unique_ptr<Thread>> threads_;	// 线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;	// 线程列表
	int initThreadSize_ = 4;	// 初始线程数量
	std::atomic_int idleThreadSize_;	//	记录空闲线程的数量 
	std::atomic_int curThreadSize_; //	记录当前线程的总数 
	int threadSizeThreshHold_;	// 线程数量上限阈值

	using Task = std::function<void()>;
	// std::queue<std::shared_ptr<Task>> taskQue_;	// 任务队列
	std::queue<Task> taskQue_;	// 任务队列

	std::atomic_int taskSize_;	// 任务数量
	int taskQueMaxThreshHold_;	// 任务队列数量上限阈值

	std::mutex taskQueMtx;	// 保证任务队列线程安全
	std::condition_variable notFull_;	// 任务队列不满
	std::condition_variable notEmpty_;	// 任务队列不空
	std::condition_variable exitCond_;	//等待线程资源全部回收

	PoolMode poolMode_;	// 当前线程池的工作模式
	std::atomic_bool isPoolRuning_;	//	当前线程池的启动状态


};

#endif // !THREADPOOL_H
