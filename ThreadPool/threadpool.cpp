#include"threadpool.h"

//Thread类构造函数
Thread::Thread(ThreadFunc _func) :func_(_func) {

}

//Thread类析构函数
Thread::~Thread() {

}

//Thread执行线程函数
void Thread::start() {
	std::thread t(func_);  //定义线程，传入其执行的函数
	t.detach();            //线程分离
}




//ThreadPool构造函数
ThreadPool::ThreadPool() :
	taskSize_(0),
	threadInitSize_(THREAD_INIT_SIZE),
	taskQueMaxHold_(TASK_MAX_THRESHOLD),
	mode(PoolMode::FIXED_MODE) {

}

//ThreadPool析构函数
ThreadPool::~ThreadPool() {

}

//设置任务队列最大任务数量
void ThreadPool::SetTaskQueMaxHold(int maxhold) {
	this->taskQueMaxHold_ = maxhold;
}

//设置线程池模式
void ThreadPool::SetPoolMode(PoolMode _mode) {
	this->mode = _mode;
}

//启动线程池
void ThreadPool::start(int size) {
	//初始化线程数量
	this->threadInitSize_ = size;
	std::cout << "size=" << size << std::endl;
	//初始化线程
	for (int i = 0; i < threadInitSize_; i++)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threads_.emplace_back(std::move(ptr));
	}

	for (int i = 0; i < threads_.size(); i++)
	{
		threads_[i]->start();
	}
}

//向任务队列中push数据
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {

	//获取锁
	std::unique_lock<std::mutex>_lock(this->mtxQue_);
	//代表此时已经满了,我们在条件变量not_Full下面等待2s,看是否不满
	if ((not_Full.wait_for(_lock, std::chrono::seconds(1), [&]()->bool {
		return taskQue_.size() < taskQueMaxHold_;
		})) == false)
	{
		LOG(WARNING, "TaskQue is full!");
		return   Result(sp, false);
	}
		taskQue_.push(sp);
		taskSize_++;
		//此时放了新任务，在notEmpty上进行通知其它消费者线程进行消费
		not_Empty.notify_all();
		return Result(sp);

}

void ThreadPool::threadFunc() {

	for (;;)
	{
		std::shared_ptr<Task>task;
		{
			std::unique_lock<std::mutex>_lock(this->mtxQue_);
			std::cout << std::this_thread::get_id() << "获取任务..." << std::endl;
			while (taskQue_.empty())
			{
				not_Empty.wait(_lock);
			}
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			std::cout << std::this_thread::get_id() << "获取任务成功" << std::endl;
			if (taskQue_.size() > 0)
			{
				not_Empty.notify_all();
			}
			//消费之后说明队列不满了
			not_Full.notify_all();
		}
		if (task != nullptr)
		{
			task->exec();
		}
	}
	std::cout << std::this_thread::get_id() << std::endl;
}



//Result默认构造函数
Result::Result(std::shared_ptr<Task>sp, bool isVaild) :
	task_(sp),
	isVaild_(isVaild)
{
	task_->SetResult(this);
}

//Result::get用户调用这个函数获得返回值
Any Result::get()
{
	if (!isVaild_)
	{
		return "";
	}
	sem_.wait();  //task任务如果没有执行完，这里会阻塞用户线程
	return std::move(any_);
}

//Result::setVal子线程调用该函数设置返回值any
void Result::SetVal(Any any)
{

	any_ = std::move(any);
	sem_.post();
}