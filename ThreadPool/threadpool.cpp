#include"threadpool.h"

//Thread�๹�캯��
Thread::Thread(ThreadFunc _func) :func_(_func) {

}

//Thread����������
Thread::~Thread() {

}

//Threadִ���̺߳���
void Thread::start() {
	std::thread t(func_);  //�����̣߳�������ִ�еĺ���
	t.detach();            //�̷߳���
}




//ThreadPool���캯��
ThreadPool::ThreadPool() :
	taskSize_(0),
	threadInitSize_(THREAD_INIT_SIZE),
	taskQueMaxHold_(TASK_MAX_THRESHOLD),
	mode(PoolMode::FIXED_MODE) {

}

//ThreadPool��������
ThreadPool::~ThreadPool() {

}

//����������������������
void ThreadPool::SetTaskQueMaxHold(int maxhold) {
	this->taskQueMaxHold_ = maxhold;
}

//�����̳߳�ģʽ
void ThreadPool::SetPoolMode(PoolMode _mode) {
	this->mode = _mode;
}

//�����̳߳�
void ThreadPool::start(int size) {
	//��ʼ���߳�����
	this->threadInitSize_ = size;
	std::cout << "size=" << size << std::endl;
	//��ʼ���߳�
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
		return Result(sp);

}

void ThreadPool::threadFunc() {

	for (;;)
	{
		std::shared_ptr<Task>task;
		{
			std::unique_lock<std::mutex>_lock(this->mtxQue_);
			std::cout << std::this_thread::get_id() << "��ȡ����..." << std::endl;
			while (taskQue_.empty())
			{
				not_Empty.wait(_lock);
			}
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
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
	}
	std::cout << std::this_thread::get_id() << std::endl;
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