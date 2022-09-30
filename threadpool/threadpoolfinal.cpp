// threadpoolfinal.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <functional>
#include <thread>
#include <future>

#include<fstream>
#include<string>
#include<vector>

#include "threadpool.h"

using namespace std;

/*
1、如何让线程提交任务更方便
submitTask1(int a, int b);
submitTask2(int a, int b, int c)
submitTask():可变参模板编程

2、C++线程库 thread packaged_task(function函数对象) async
使用future 代替Result 减少线程池代码量
*/


int sum1(int a, int b)
{
    this_thread::sleep_for(chrono::seconds(2));
    return (a + b);
}

int sum2(int a, int b, int c)
{
    return (a + b + c);
}




int main()
{
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_FIXED);
        pool.start(4);

        future<int> R1 = pool.submitTask(sum1, 5, 8);

        future<int> R2 = pool.submitTask(sum1, 6, 8);
        future<int> R3 = pool.submitTask(sum1, 3, 8);
        future<int> R4 = pool.submitTask([](int b, int e)->int {
            int sum = 0;
            for (int i = b; i <= e; i++)
                sum += sum;
            return sum;
            }, 100, 8);

        cout << R1.get() << endl;
        cout << R2.get() << endl;
        cout << R3.get() << endl;
        cout << R4.get() << endl;

		std::cout << "Hello World!\n";
		getchar();
    }
    //packaged_task<int(int, int)> task(sum1);
    //// future <=> Result
    //future<int> res = task.get_future();
    //task(1, 8);

    //cout << res.get() << endl;
    /*thread td1(sum1, 1, 2);
    td1.join();*/

    std::cout << "Hello World!\n";
    getchar();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
