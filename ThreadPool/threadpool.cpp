#include"threadpool.h"

//Thread�๹�캯��
Thread::Thread(ThreadFunc _func) :func_(_func),id(threadId++) {

}

//Thread����������
Thread::~Thread() {

}

//Threadִ���̺߳���
void Thread::start() {
	std::thread t(func_,id);  //�����̣߳�������ִ�еĺ���
	t.detach();            //�̷߳���
}
std::atomic_int Thread::threadId = 0;


//���ع涨���߳�ֵ
int Thread::GetId()
{
	return this->id;
}


//ThreadPool���캯��
ThreadPool::ThreadPool() :
	taskSize_(0),
	threadInitSize_(THREAD_INIT_SIZE),
	taskQueMaxHold_(TASK_MAX_THRESHOLD),
	threadsMaxSize_(THREAD_CACHED_MAX_SIZE),
	curthreadSize_(0),
	freeThreadSize_(0),
	mode(PoolMode::FIXED_MODE),
	isRunning(false) {

}

//ThreadPool��������
ThreadPool::~ThreadPool() {

}

//����������������������
void ThreadPool::SetTaskQueMaxHold(int maxhold) {
	if (isPoolRunning())
	{
		LOG(WARNING, "ThreadPool is already running,can not set Taskque max size");
		return;
	}
	this->taskQueMaxHold_ = maxhold;
}

//�����̳߳�ģʽ
void ThreadPool::SetPoolMode(PoolMode _mode) {
	if (isPoolRunning())
	{
		LOG(WARNING, "ThreadPool is already running,can not set ThreadPool Mode");
		return;
	}
	this->mode = _mode;
}

//����cachedģʽ����߳�����
void ThreadPool::SetThreadsMaxSize(int size)
{
	//�ж��̳߳��Ƿ��Ѿ���ʼʹ��
	if (isPoolRunning())
	{

		LOG(WARNING, "ThreadPool is already running,can not set cached Threads Max Size");
		return;
	}
	this->threadsMaxSize_ = size;
}


bool ThreadPool::isPoolRunning()
{
	return isRunning;
}

//�����̳߳�
void ThreadPool::start(int size) {
	//��ʼ���߳�����
	//�ж��Ƿ�Ϊcachedģʽ����cachedģʽ���̳߳�ʼ����������С������߳������������ʼ���߳�������������߳�
	//�����ֶ��ı�����Ĭ��ֵ
	if (this->mode == PoolMode::CACHED_MODE)
	{
		if (size > this->threadsMaxSize_)
		{
			LOG(WARNING, "ThreadPool::CACHED_MODE threadInitSize must less than threadsMaxSize_,change threadInitSize become default");
			this->threadInitSize_ = THREAD_INIT_SIZE;
		}
		else
		{
			this->threadInitSize_ = size;
		}
	}
	else
	{
		this->threadInitSize_ = size;
	}

	std::cout << "size=" << size << std::endl;

	//�����̳߳�״̬
	this->isRunning = true;
	//��ʼ���߳�
	for (int i = 0; i < threadInitSize_; i++)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		threads_.emplace(ptr->GetId(),std::move(ptr));
		this->curthreadSize_++;          //��ǰ�߳�����++
		this->freeThreadSize_++;         //ʣ���߳�����++
	}

	for (int i = 0; i < threadInitSize_; i++)
	{
		threads_[i]->start();
	}
}

//�����������push����
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {

	//��ȡ��
	std::unique_lock<std::mutex>_lock(this->mtxQue_);
	//�����ʱ�Ѿ�����,��������������not_Full����ȴ�2s,���Ƿ���
	if ((not_Full.wait_for(_lock, std::chrono::seconds(1), [&]()->bool {
		return taskQue_.size() < taskQueMaxHold_;
		})) == false)
	{
		LOG(WARNING, "TaskQue is full!");
		return   Result(sp, false);
	}
		taskQue_.push(sp);
		taskSize_++;
		//��ʱ������������notEmpty�Ͻ���֪ͨ�����������߳̽�������
		not_Empty.notify_all();

		//�����cachedģʽ����ǰ�߳����������������������߳������������cached�߳�����
		//���Ǿʹ������߳�
		if (this->mode == PoolMode::CACHED_MODE && taskSize_>curthreadSize_ && curthreadSize_ < threadsMaxSize_)
		{
			//��ǰ�߳�����::��������ִ��������߳����� 
			//�����߳�����::��ִ��������߳�����
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int id = ptr->GetId();
			threads_.emplace(ptr->GetId(), std::move(ptr));
			this->curthreadSize_++;          //��ǰ�߳�����++
			this->freeThreadSize_++;         //ʣ���߳�����++
			threads_[id]->start(); //�����߳�
			LOG(INFO, "Add a new thread:" + std::to_string(id));
		}
		return Result(sp);

}

void ThreadPool::threadFunc(int threadId) {
	auto LastTime = std::chrono::high_resolution_clock().now();
	printf("threadId=%d\n", threadId);
	while(isPoolRunning())
	{
		std::shared_ptr<Task>task;
		{
			std::unique_lock<std::mutex>_lock(this->mtxQue_);
			std::cout << std::this_thread::get_id() << "��ȡ����..." << std::endl;
			while (taskQue_.empty())
			{
				if (mode == PoolMode::CACHED_MODE)
				{
					//�����ʱ����ʱ�䳬ʱ��
					if (std::cv_status::timeout == not_Empty.wait_for(_lock, std::chrono::seconds(2)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now-LastTime);
						//����60s����ʱ��ɾ���ýڵ�
						if (dur.count() > 5)
						{
							threads_.erase(threadId);
							curthreadSize_--;   //��ǰ�߳�����--
							freeThreadSize_--;  //�����߳�����--
							std::stringstream ss;
							ss << std::this_thread::get_id();
							ss << " deleted";
							LOG(INFO, ss.str());
							return;
						}

					}
				}
				else
				{
					not_Empty.wait(_lock);
				}
				if (!isPoolRunning())
				{
					threads_.erase(threadId);
					curthreadSize_--;   //��ǰ�߳�����--
					freeThreadSize_--;  //�����߳�����--
					std::stringstream ss;
					ss << "thread:";
					ss << std::this_thread::get_id();
					ss << " deleted";
					LOG(INFO, ss.str());
					return;
				}
			}
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			freeThreadSize_--;   //ִ������ʱ�����߳�--

			std::cout << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;
			if (taskQue_.size() > 0)
			{
				not_Empty.notify_all();
			}
			//����֮��˵�����в�����
			not_Full.notify_all();
		}
		if (task != nullptr)
		{
			task->exec();
		}
		//ִ��������֮�������������++
		freeThreadSize_++;
		LastTime = std::chrono::high_resolution_clock().now();
	}
	std::cout << std::this_thread::get_id() << std::endl;
	threads_.erase(threadId);
	curthreadSize_--;   //��ǰ�߳�����--
	freeThreadSize_--;  //�����߳�����--
	std::stringstream ss;
	ss << "thead:";
	ss << std::this_thread::get_id();
	ss << " deleted";
	LOG(INFO, ss.str());
	return;
}



//ResultĬ�Ϲ��캯��
Result::Result(std::shared_ptr<Task>sp, bool isVaild) :
	task_(sp),
	isVaild_(isVaild)
{
	task_->SetResult(this);
}

//Result::get�û��������������÷���ֵ
Any Result::get()
{
	if (!isVaild_)
	{
		return "";
	}
	sem_.wait();  //task�������û��ִ���꣬����������û��߳�
	return std::move(any_);
}

//Result::setVal���̵߳��øú������÷���ֵany
void Result::SetVal(Any any)
{
	any_ = std::move(any);
	sem_.post();
}