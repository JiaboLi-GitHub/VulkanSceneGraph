/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/UpdateOperations.h>

using namespace vsg;

// UpdateOperations类的构造函数
// 创建更新操作管理器，用于管理每帧需要执行的操作
UpdateOperations::UpdateOperations()
{
}

// UpdateOperations类的析构函数
UpdateOperations::~UpdateOperations()
{
}

// 添加更新操作
// 将操作添加到更新操作列表中
// op: 要添加的操作
// runBehavior: 运行行为（ONE_TIME表示只运行一次，ALL_FRAMES表示每帧都运行）
void UpdateOperations::add(ref_ptr<Operation> op, RunBehavior runBehavior)
{
    std::scoped_lock<std::mutex> lock(_updateOperationMutex);
    // 根据运行行为将操作添加到相应的列表
    if (runBehavior == ONE_TIME)
        _updateOperationsOneTime.push_back(op);  // 一次性操作
    else
        _updateOperationsAllFrames.push_back(op);  // 每帧操作
}

// 移除更新操作
// 从更新操作列表中移除指定的操作
// op: 要移除的操作
void UpdateOperations::remove(ref_ptr<Operation> op)
{
    std::scoped_lock<std::mutex> lock(_updateOperationMutex);
    // 从两个列表中移除操作
    _updateOperationsOneTime.remove(op);
    _updateOperationsAllFrames.remove(op);
}

// 清空所有更新操作
// 清除所有一次性操作和每帧操作
void UpdateOperations::clear()
{
    std::scoped_lock<std::mutex> lock(_updateOperationMutex);
    _updateOperationsOneTime.clear();
    _updateOperationsAllFrames.clear();
}

// 获取一次性更新操作
// 返回所有一次性更新操作的副本
// 返回值：一次性更新操作列表
UpdateOperations::container_type UpdateOperations::getUpdateOperationsOneTime() const
{
    std::scoped_lock<std::mutex> lock(_updateOperationMutex);
    return _updateOperationsOneTime;
}

// 获取每帧更新操作
// 返回所有每帧更新操作的副本
// 返回值：每帧更新操作列表
UpdateOperations::container_type UpdateOperations::getUpdateOperationsAllFrames() const
{
    std::scoped_lock<std::mutex> lock(_updateOperationMutex);
    return _updateOperationsAllFrames;
}

// 运行所有更新操作
// 执行所有一次性操作（执行后清除）和每帧操作
void UpdateOperations::run()
{
    container_type updateOperations;

    {
        std::scoped_lock<std::mutex> lock(_updateOperationMutex);
        // 交换一次性操作列表（执行后会被清空）
        _updateOperationsOneTime.swap(updateOperations);
        // 将每帧操作添加到执行列表
        updateOperations.insert(updateOperations.end(), _updateOperationsAllFrames.begin(), _updateOperationsAllFrames.end());
    }

    // 执行所有操作
    for (auto op : updateOperations) op->run();
}
