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

//��Ҫ����ħ������
const int TASK_MAX_THRESHHOLD = 2;//INT32_MAX; // ��������������ֵ
const int THREAD_MAX_THRESHHOLD = 10;	// �߳�����������ֵ
const int THREAD_MAX_IDLE_TIME = 60;	// ��λ����

// �̳߳�֧������
enum class PoolMode
{
	MODE_FIXED,	// �̶��߳�����
	MODE_CACHED, // �ɶ�̬�����߳�����
};

// �߳�����
class Thread
{
public:
	// �̺߳�����������
	using ThreadFun = std::function<void(int)>;

	// ����
	Thread(ThreadFun func)
		: func_(func)
		, threadId_(generateId_++)
	{}
	// ����
	~Thread() = default;

	// �߳�����
	void start()
	{
		//����һ���߳���ִ���߳�����
		std::thread t(func_, threadId_); //C++��˵ �̶߳���t �̺߳���func_
		t.detach(); //���÷����߳�
	}

	// ��ȡ�߳�Id
	int getId() const
	{
		return threadId_;
	}

private:
	ThreadFun func_;

	static int generateId_;
	int threadId_;	// �����߳�Id
};
int Thread::generateId_ = 0;

// �̳߳�
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

		// �ȴ��̳߳ص������̳߳ط���
		std::unique_lock<std::mutex> myLock(taskQueMtx);
		notEmpty_.notify_all();

		exitCond_.wait(myLock, [&]()->bool {return threads_.size() == 0; });
	}

	// �����̳߳صĹ���ģʽ
	void setMode(PoolMode mode)
	{
		if (checkRuningState())
			return;

		poolMode_ = mode;
	}

	//���ó�ʼ�߳�����
	void setInitThreadSixe(int size);

	// ���������������������ֵ
	void setTaskQueMaxThreshHol(int threshHold_)
	{
		if (checkRuningState())
			return;

		if (poolMode_ == PoolMode::MODE_CACHED)
			taskQueMaxThreshHold_ = threshHold_;
	}
	// �����߳�����������ֵ
	void setThreadSizeThreshHol(int threshHold_)
	{
		if (checkRuningState())
			return;

		threadSizeThreshHold_ = threshHold_;
	}

	// ���߳��ύ����
	// ʹ�ÿɱ��ģ���̣���submitTask���Խ������⺯����������������
	// Result submitTask(std::shared_ptr<Task> sp);
	// ����ֵfuture<����ֵ����>
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		//������񣬷ŵ������������
		using RType = decltype(func(args...));
		auto task = std::make_shared < std::packaged_task<RType() >>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

		// ��ȡ��
		std::unique_lock<std::mutex> myMtx(taskQueMtx);

		if (!notFull_.wait_for(myMtx, std::chrono::seconds(1),
			[&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; }))
		{
			// �ȴ�һ����������Ȼû������
			std::cerr << "task queue is full submitTask fail!" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType { return RType(); });
			(*task)();
			return task->get_future();
		}

		// ����п��� ������ŵ����������
		//taskQue_.emplace(sp);
		//using Task = std::function<void()>;
		taskQue_.emplace([task]() {(*task)(); });
		taskSize_++;

		// ������ŵ���������к� notEmpty_֪ͨ�̳߳ط����߳�ִ������
		notEmpty_.notify_all();

		// cachedģʽ �������������Ϳ����߳��������ж��Ƿ���Ҫ�����µ��߳�
		//����С����
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ < threadSizeThreshHold_)
		{
			//�������߳�
			std::cout << ">>>>>>>create new thread!" << std::endl;
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			//�����߳�
			threads_[threadId]->start();
			//�޸��߳���ر���
			curThreadSize_++;
			idleThreadSize_++;
		}

		return result;
	}
	// �����̳߳�
	void start(int initThreadSize = std::thread::hardware_concurrency()/*���ص�ǰϵͳ��CPU������*/)
	{
		//�����߳�����ת̬
		isPoolRuning_ = true;

		// ��¼��ʼ�߳�����
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;

		// �����̶߳���
		for (int i = 0; i < initThreadSize_; i++)
		{
			// ����Thread�̶߳����ʱ�򣬰��̺߳�������thread����
			// std::unique_ptr<Thread>
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			// threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			// threads_.emplace_back(std::move(ptr));
		}

		// ���������߳� std::vector<Thread*> threads_;
		for (int i = 0; i < initThreadSize_; i++)
		{
			threads_[i]->start(); // ��Ҫȥִ��һ���̺߳���
			idleThreadSize_++;	//��¼��ʼ�����߳�����
		}
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//�����̺߳���
	void threadFunc(int threadId)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();

		for (;;)
			//while(isPoolRuning_)
		{
			//std::shared_ptr<Task> task;
			Task task;
			{
				// �Ȼ�ȡ��
				std::unique_lock<std::mutex> myMtx(taskQueMtx);
				std::cout << "tid= " << std::this_thread::get_id() << "���Ի�ȡ����..... " << std::endl;

				//ÿ�뷵��1��,��ô���֣���ʱ����  �����������ִ�з���
				while (/*isPoolRuning_ && */taskQue_.size() == 0) //�� + ˫���ж�
				{
					if (!isPoolRuning_)
					{
						//�̳߳�Ҫ����������Դ
						//�̳߳�ִ��������Ҫ����������Դ
						threads_.erase(threadId);	// ���������߳�
						std::cout << "threadId:" << std::this_thread::get_id() << "exit!" << std::endl;
						exitCond_.notify_all();
						return;
					}

					//cachedģʽ�� �ѿ���ʱ�䳬��60s�Ŀ����̣߳�����initThreadSize_�������̣߳�����
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						// �������� ��ʱ����
						if (std::cv_status::timeout ==
							notEmpty_.wait_for(myMtx, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
							{
								// ��ʼ���յ�ǰ�߳�
								// ��¼�߳���ر�����ֵ�޸�
								// ���̶߳�����߳��б�������ɾ��
								// threadId->thread����->ɾ��
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
						// �ȴ�notEmpty����
						notEmpty_.wait(myMtx, [&]()->bool { return taskQue_.size() > 0; });
					}
				}
				idleThreadSize_--;	//	�߳�ȡ����ִ��

				std::cout << "tid= " << std::this_thread::get_id() << "��ȡ����ɹ�..... " << std::endl;

				// �����������ȡһ���������
				task = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				// �����ʣ������֪ͨ�����߳�ִ������
				if (taskQue_.size() > 0)
				{
					notEmpty_.notify_all();
				}

				// ȡ��һ�����񣬽���֪ͨ ���Լ����ύ����
				notFull_.notify_all();

			}   // ��������Ϳ��԰����ͷŵ�

			// ��ǰ�̸߳���ִ���������
			if (task != nullptr)
			{
				//task->run();
				//task->exec();
				task(); // ִ��function<void()>
			}

			idleThreadSize_++; //ִ�������� תΪ�����߳�
			auto lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
		}

		////�̳߳�ִ��������Ҫ����������Դ
		//threads_.erase(threadId);	// ���������߳�
		//std::cout << "threadId:" << std::this_thread::get_id() << "exist!" << std::endl;
		//exitCond_.notify_all();
		//return;
	}

	//���pool������״̬
	bool checkRuningState() const
	{
		return isPoolRuning_;
	}

private:
	//std::vector<std::unique_ptr<Thread>> threads_;	// �߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;	// �߳��б�
	int initThreadSize_ = 4;	// ��ʼ�߳�����
	std::atomic_int idleThreadSize_;	//	��¼�����̵߳����� 
	std::atomic_int curThreadSize_; //	��¼��ǰ�̵߳����� 
	int threadSizeThreshHold_;	// �߳�����������ֵ

	using Task = std::function<void()>;
	// std::queue<std::shared_ptr<Task>> taskQue_;	// �������
	std::queue<Task> taskQue_;	// �������

	std::atomic_int taskSize_;	// ��������
	int taskQueMaxThreshHold_;	// �����������������ֵ

	std::mutex taskQueMtx;	// ��֤��������̰߳�ȫ
	std::condition_variable notFull_;	// ������в���
	std::condition_variable notEmpty_;	// ������в���
	std::condition_variable exitCond_;	//�ȴ��߳���Դȫ������

	PoolMode poolMode_;	// ��ǰ�̳߳صĹ���ģʽ
	std::atomic_bool isPoolRuning_;	//	��ǰ�̳߳ص�����״̬


};

#endif // !THREADPOOL_H
