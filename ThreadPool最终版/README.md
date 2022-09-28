# 基于C++11可变参模板实现的线程池

## 项目描述

基于c++11可变参模板实现的线程池，支持任意函数和任意参数的传递，可获取线程池执行完函数的返回值，支持fixed模式和cached模式，使用方便，可跨平台

> 1.基于可变参模板编程和引用折叠原理，实现线程池submitTask接口，支持任意任务函数和任意参数的传递
>
> 2.使用future类型定制submitTask提交任务的返回值
>
> 3.使用unordered_map和queue容器管理线程对象和任务
>
> 4.基于条件变量condition_variable和互斥锁mutex实现任务提交线程和任务执行线程间的通信机制
>
> 5.支持fixed和cached模式的线程池定制
>
> 6.可跨平台使用

##### 平台工具：

vs2022开发，centos7下vim/g++开发，gdb分析定位死锁问题

## 使用说明

> 只需在项目工程中包含threadpool.h头文件即可
>
> 本线程池分为两种模式：fixed模式和cached模式，fixed模式即固定线程数量的线程池，cached即线程会随着任务数量变化的线程池

### 基本接口介绍

> 设置线程池中任务队列的最大值

```cpp
//设置任务队列最大值，默认为UINT32——MAX
void SetTaskQueMaxSize(int size = TASKQUE_MAX_SIZE_DEFAULT)
```

> 设置线程池模式

```cpp
//设置线程池模式，默认为fixed模式，即固定线程数量的线程池
void SetThreadPoolMode(PoolMode mode = PoolMode::FIXED_MODE)
```

> 设置cached模式下线程的最大值

```cpp
//设置cached模式下线程的最大值，在cached模式中线程动态增长，但是线程的增长必须是有上限的，不能无限制增长，必须限制线程池中线程最大值
void SetThreadsMaxSize(int size = THREAD_MAX_SIZE_DEFAULT)
```

> 启动线程池

```cpp
//start启动线程池，初始化线程数量，默认初始化线程数量为cpu核心数量
void start(int size = std::thread::hardware_concurrency())
```

> 向线程池提交任务

```cpp
//向线程池提交任务
auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
```

### fixed模式

```cpp
#include"ThreadPool"
//定义线程池函数
double sum1(double l,double r)
{
    dounle sum=0;
    for(double i=l;i<=r;i++)
    {
        sum+=i;
    }
    return sum;
}
int main()
{
    ThreadPool p;//定义线程池对象，默认为CACHED模式
    p.start(4);  //启动线程池，初始化4个线程
    
    //提交任务时就和向thread线程提交对象一样p.submitTask(sum1,1,100)
    //即可向线程池提交任务，线程池执行完任务之后利用future机制获取线程池执行完的结果future对象，利用future的get方法即可得到返回值
    std::future<double>res1=p.submitTask(sum1,1,100);
    cout<<res1.get()<<endl;
}
```

### cached模式

> 在cached模式中，我们初始时创建一批线程，并且规定线程池最大线程数量，当我们提交任务过多时，线程池则会自动增加线程去执行任务，但是线程的数量最大不超过最大线程数量，当线程执行完任务长时间处于空闲状态，超过一段时间后，就会自动杀死空闲线程，最大限度使用系统资源

```cpp
#include"ThreadPool"
//定义线程池函数
double sum1(double l,double r)
{
    dounle sum=0;
    for(double i=l;i<=r;i++)
    {
        sum+=i;
    }
    return sum;
}
int main()
{
    ThreadPool p;//定义线程池对象，默认为CACHED模式
    p.SetThreadPoolMode(PoolMode::CACHED_MODE);//设置线程池为cached模式
    p.SetThreadsMaxSize(20);   //设置cached模式下线程最大值
    p.start(4);  //启动线程池，初始化4个线程
    
    //提交任务时就和向thread线程提交对象一样p.submitTask(sum1,1,100)
    //即可向线程池提交任务，线程池执行完任务之后利用future机制获取线程池执行完的结果future对象，利用future的get方法即可得到返回值
    std::future<double>res1=p.submitTask(sum1,1,100);
    cout<<res1.get()<<endl;
}
```

### 线程池析构

> 在ThreadPool对象出作用域后会自动调用析构函数将其析构，但是如果此时任务队列中依然有任务没有执行完，则会等待任务队列中的任务执行完之后再进行析构，保证任务都被执行完毕

## 性能分析

> 我们利用该线程池执行10亿到70亿的大数加法，在线程池中我们将这个任务进行分解，分解成10—20亿，20–30亿…….60–70亿，而主线程则直接从10亿加到70亿，对比其所用时间，测试代码如下所示

```cpp
#include"threadpool.h"

double sum1(double l, double r)
{
	double sum = 0;
	for (double i = l; i <= r; i++)
	{
		sum += i;
	}
	return sum;
}
int main()
{
	ThreadPool p;     //创建线程池对象
	p.SetThreadPoolMode(PoolMode::CACHED_MODE);   //设置为cached模式
	p.start(4);     //初始化4个线程
	auto LasTime = std::chrono::high_resolution_clock().now();   //标记起始执行时间
	std::future<double>res1 = p.submitTask(sum1, 1000000000, 2000000000);  //向线程池提交6个任务
	std::future<double>res2 = p.submitTask(sum1, 2000000001, 3000000000);
	std::future<double>res3 = p.submitTask(sum1, 3000000001, 4000000000);
	std::future<double>res4 = p.submitTask(sum1, 4000000001, 5000000000);
	std::future<double>res5 = p.submitTask(sum1, 5000000001, 6000000000);
	std::future<double>res6 = p.submitTask(sum1, 6000000001, 7000000000);
    //得到任务的返回值
	double sum1 = res1.get();
	double sum2 = res2.get();
	double sum3 = res3.get();
	double sum4 = res4.get();
	double sum5 = res5.get();
	double sum6 = res6.get();
	auto now = std::chrono::high_resolution_clock().now();   //执行完6个任务所用的时间
	auto time = std::chrono::duration_cast<std::chrono::seconds>(now - LasTime);  //计算时间差
	cout << "sum_1=" << sum1 + sum2 + sum3 + sum4 + sum5 + sum6 << endl;
	cout << "time_1=" << time.count() << "s" << endl;

	LasTime = std::chrono::high_resolution_clock().now();
    //单线程直接执行任务
	double sum_2 = 0;
	for (double i = 1000000000; i <= 7000000000; i++)
	{
		sum_2 += i;
	}
	now = std::chrono::high_resolution_clock().now();
	time = std::chrono::duration_cast<std::chrono::seconds>(now - LasTime);
	cout << "sum_2=" << sum_2 << endl;
	cout << "time_2=" << time.count() << "s" << endl;
	return 0;
}
```

![image-20220928163558996](https://raw.githubusercontent.com/qingyan520/Cloud_img/master/img/image-20220928163558996.png)

> 如上所示，几乎快了近4倍左右，