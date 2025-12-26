/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/threading/OperationThreads.h>

using namespace vsg;

// OperationThreads类的构造函数
// 创建操作线程池，用于并行执行操作
// numThreads: 线程数量
// in_status: 活动状态对象，用于控制线程的生命周期
OperationThreads::OperationThreads(uint32_t numThreads, ref_ptr<ActivityStatus> in_status) :
    status(in_status)
{
    // 如果没有提供状态对象，创建一个新的
    if (!status) status = ActivityStatus::create();
    // 创建操作队列
    queue = OperationQueue::create(status);

    // 定义线程运行函数：从队列中取出操作并执行
    auto runThread = [](ref_ptr<OperationQueue> q, ref_ptr<ActivityStatus> thread_status) {
        // 只要线程状态为活动，就持续从队列中取出操作并执行
        while (thread_status->active())
        {
            ref_ptr<Operation> operation = q->take_when_available();
            if (operation)
            {
                operation->run();
            }
        }
    };

    // 创建指定数量的工作线程
    for (size_t i = 0; i < numThreads; ++i)
    {
        threads.emplace_back(runThread, queue, status);
    }
}

// OperationThreads类的析构函数
// 停止所有线程
OperationThreads::~OperationThreads()
{
    stop();
}

// 在当前线程中运行操作
// 从队列中取出操作并在当前线程中执行（用于单线程模式）
void OperationThreads::run()
{
    while (ref_ptr<Operation> operation = queue->take())
    {
        operation->run();
    }
}

// 停止所有操作线程
// 设置状态为非活动，并等待所有线程结束
void OperationThreads::stop()
{
    // 设置状态为非活动，通知所有线程停止
    status->set(false);

    // 等待所有线程结束
    for (auto& thread : threads)
    {
        thread.join();
    }

    // 清空线程列表
    threads.clear();
}
