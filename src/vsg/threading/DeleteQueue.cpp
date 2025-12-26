/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Options.h>
#include <vsg/threading/DeleteQueue.h>
#include <vsg/ui/FrameStamp.h>

using namespace vsg;

/////////////////////////////////////////////////////////////////////////
//
// DeleteQueue - 延迟删除队列，用于安全地延迟删除Vulkan对象
//

// DeleteQueue类的构造函数
// 创建延迟删除队列，用于在多线程环境中安全地延迟删除对象
// status: 活动状态对象，用于控制删除线程的生命周期
DeleteQueue::DeleteQueue(ref_ptr<ActivityStatus> status) :
    _status(status)
{
}

// DeleteQueue类的析构函数
DeleteQueue::~DeleteQueue()
{
}

// 推进删除队列
// 更新当前帧计数，并通知删除线程可以处理到期的对象
// frameStamp: 帧戳对象，包含当前帧计数
void DeleteQueue::advance(ref_ptr<FrameStamp> frameStamp)
{
    std::scoped_lock lock(_mutex);

    // 更新当前帧计数
    frameCount = frameStamp->frameCount;

    // 如果有对象可以删除（帧计数已到期），通知删除线程
    if (!_objectsToDelete.empty() && _objectsToDelete.front().frameCount <= frameStamp->frameCount)
    {
        _cv.notify_one();
    }
}

// 等待并清除到期的对象
// 等待条件变量通知，然后删除所有到期的对象
// 这是删除线程的主要工作函数
void DeleteQueue::wait_then_clear()
{
    ObjectsToDelete objectsToDelete;
    std::list<ref_ptr<SharedObjects>> sharedObjectsToPrune;

    {
        std::chrono::duration waitDuration = std::chrono::milliseconds(100);
        std::unique_lock lock(_mutex);

        uint64_t previous_frameCount = frameCount.load();

        // 等待条件变量信号，表示有操作已添加或帧计数已更新
        while ((_objectsToDelete.empty() || (frameCount.load() == previous_frameCount)) && _status->active())
        {
            _cv.wait_for(lock, waitDuration);
        }
        // 找到所有到期的对象（帧计数小于等于当前帧计数）
        auto last_itr = std::find_if(_objectsToDelete.begin(), _objectsToDelete.end(), [&](const ObjectToDelete& otd) { return otd.frameCount > frameCount; });

        // 使用容器交换来最小化互斥锁持有时间
        objectsToDelete.splice(objectsToDelete.end(), _objectsToDelete, _objectsToDelete.begin(), last_itr);

        sharedObjectsToPrune.swap(_sharedObjectsToPrune);
    }

    size_t numObjectsToDelete = objectsToDelete.size();

    // 清除到期的对象（释放引用）
    objectsToDelete.clear();

    // 如果有对象被删除，修剪共享对象缓存
    if (numObjectsToDelete > 0)
    {
        for (auto& sharedObjects : sharedObjectsToPrune)
        {
            sharedObjects->prune();
        }
        sharedObjectsToPrune.clear();
    }
}

// 立即清除所有待删除的对象
// 不等待帧计数，直接删除所有对象（用于清理）
void DeleteQueue::clear()
{
    ObjectsToDelete objectsToDelete;

    // 使用容器交换来最小化互斥锁持有时间
    {
        std::scoped_lock lock(_mutex);
        objectsToDelete.swap(objectsToDelete);
    }

    size_t numObjectsToDelete = objectsToDelete.size();

    //vsg::info("DeleteQueue::clear(), releasing ", nodesToRelease.size());
    // 清除所有对象（释放引用）
    objectsToDelete.clear();

    // 如果有对象被删除，修剪共享对象缓存
    if (numObjectsToDelete > 0)
    {
        for (auto& sharedObjects : _sharedObjectsToPrune)
        {
            sharedObjects->prune();
        }
        _sharedObjectsToPrune.clear();
    }
}
