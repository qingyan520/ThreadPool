#pragma once
#include<iostream>
#include<queue>
#include<vector>
#include<functional>
#include<memory>
#include<mutex>
#include<condition_variable>
#include<thread>
#include<chrono>
#include<string>


#define INFO 1
#define WARNING 2
#define ERROR_ 3
#define FALTA 4

#define LOG(level,message)  log(#level,message,__FILE__,__LINE__)
static void log(std::string level, std::string message, std::string file_name, int line) {
	//��ʾ��ǰ�¼�
	char now[1024] = { 0 };
	time_t tt = time(nullptr);
	struct tm* ttime;
	ttime = localtime(&tt);
	strftime(now, 1024, "%Y-%m-%d %H:%M:%S", ttime);
	// cout << "[" << now << "" << "][" << level << "]" << "[" << message << "]" << "[" << file_name << "]" << "[" << line << "]" << endl;

	printf("[%s][%s][%s][%s][%d]\n", now, level.c_str(), message.c_str(), file_name.c_str(), line);

}

const int TASK_MAX_THRESHOLD = 10;   //��������������
const int THREAD_INIT_SIZE = 8;        //�̳߳�ʼ������

//�̳߳�ģʽ���̶�ģʽ�Ͷ�̬����ģʽ
enum class PoolMode
{
	FIXED_MODE,      //�̶��������̳߳�
	CACHED_MODE      //�߳��������Զ�̬�������̳߳�
};



//Any�ࣺ���Խ����������͵ķ���ֵ
class Any
{
public:

	//ɾ����ֵ���õĿ�������͸�ֵ����
	Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;

	//������ֵ���õ��ƶ�����͸�ֵ����
	Any& operator=(Any&&) = default;
	Any(Any&&) = default;

	template<class T>
	Any(T data) :
		_ptr(std::make_unique<Derive<T>>(data))
	{

	}
	template<class T>
	T cast_()
	{
		Derive<T>* p = dynamic_cast<Derive<T>*>(_ptr.get());
		if (p == nullptr)
		{
			LOG(ERROR_, "type is incompatiable!");
			throw "type is incompatiable!";
		}
		return p->getval();

	}


private:

	class Base {
	public:
		virtual ~Base() = default;
	};
	template<class T>
	class Derive :public Base
	{
	public:
		Derive(T data) :_data(data)
		{
		}
		T getval()
		{
			return _data;
		}
	private:
		T _data;
	};

	std::unique_ptr<Base>_ptr;
};


//�ź���ģ��ʵ��
class Sempahore
{
public:

	void wait()
	{
		std::unique_lock<std::mutex>_lck(mtx_);
		cond_.wait(_lck, [&]()->bool {
			return sLimited_ > 0;
			});
		sLimited_--;
	}
	void post()
	{
		std::unique_lock<std::mutex>_lck(mtx_);
		sLimited_++;
		cond_.notify_all();
	}
private:
	int sLimited_ = 0;
	std::mutex mtx_;
	std::condition_variable cond_;
};


//ǰ������Task
class Task;
//����ֵ����
class Result
{
public:
	Result(std::shared_ptr<Task>sp, bool isVaild = true);

	//setVal��������ķ���ֵ
	void SetVal(Any any);
	//get�������û�������������������ķ���ֵ
	Any get();

private:
	Any any_;    //�洢Any����ֵ
	Sempahore sem_; //�ź���
	std::shared_ptr<Task>task_;
	bool isVaild_;
};


//������
class Task
{
public:
	//���ü̳У�ִ���̳߳��е�����
	virtual Any run() = 0;

	void exec()
	{
		if (res != nullptr)
			res->SetVal(run());
	}


	void SetResult(Result* res_)
	{
		res = res_;
	}


private:
	Result* res = nullptr;
};

//�߳���
class Thread
{
public:
	using ThreadFunc = std::function<void()>;
	Thread(ThreadFunc _func);
	~Thread();

	void start();
private:
	Thread::ThreadFunc func_;
};
//�̳߳�
class ThreadPool
{
public:
	ThreadPool();             //���캯��
	~ThreadPool();            //��������

	void SetTaskQueMaxHold(int maxHold = TASK_MAX_THRESHOLD);  //�������������������
	void SetPoolMode(PoolMode _mode);     //�����̳߳�ģʽ��Ĭ��ΪFIXED_MODE

	Result submitTask(std::shared_ptr<Task> sp);  //������������ύ����
	void start(int size = THREAD_INIT_SIZE);                          //�����̳߳�

	ThreadPool& operator=(const ThreadPool&) = delete; //��ֹ�̳߳ؽ��п�������͸�ֵ����
	ThreadPool(const ThreadPool&) = delete;
private:
	std::vector<std::unique_ptr<Thread>>threads_;    //�߳��������洢�߳�
	std::queue<std::shared_ptr<Task>>taskQue_;  //������У��洢����
	std::atomic_int taskSize_;      //��¼������������������
	int threadInitSize_;            //�̳߳�ʼ����
	int taskQueMaxHold_;            //��������������ֵ

	std::mutex mtxQue_;            //��֤��������̰߳�ȫ
	std::condition_variable not_Empty;   //������в���
	std::condition_variable not_Full;    //������в���

	PoolMode mode;                    //�����̳߳ص�ģʽ

private:
	void threadFunc();               //�̺߳���
};
