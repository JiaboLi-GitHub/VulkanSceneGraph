/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/RecordAndSubmitTask.h>
#include <vsg/app/View.h>
#include <vsg/io/Logger.h>
#include <vsg/ui/ApplicationEvent.h>
#include <vsg/vk/State.h>

using namespace vsg;

// RecordAndSubmitTask类的构造函数
// 创建记录和提交任务，用于管理命令缓冲区的记录和提交
// in_device: Vulkan设备对象
// numBuffers: 缓冲区数量（用于多缓冲）
RecordAndSubmitTask::RecordAndSubmitTask(Device* in_device, uint32_t numBuffers) :
    device(in_device)  // Vulkan设备
{
    CPU_INSTRUMENTATION_L1(instrumentation);

    _currentFrameIndex = numBuffers; // numBuffers用于表示未设置的值
    // 初始化索引向量
    for (uint32_t i = 0; i < numBuffers; ++i)
    {
        _indices.emplace_back(numBuffers); // numBuffers用于表示未设置的值
    }

    // 创建围栏向量
    _fences.resize(numBuffers);
    for (uint32_t i = 0; i < numBuffers; ++i)
    {
        _fences[i] = Fence::create(device);
    }

    // 创建传输任务
    transferTask = TransferTask::create(in_device, numBuffers);

    // 创建传输完成信号量
    earlyTransferConsumerCompletedSemaphore = Semaphore::create(in_device);
    lateTransferConsumerCompletedSemaphore = Semaphore::create(in_device);
}

// 推进帧索引
// 更新当前帧索引，并移动历史帧索引
void RecordAndSubmitTask::advance()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "RecordAndSubmitTask advance", COLOR_VIEWER);

    // info("\nRecordAndSubmitTask::advance()");

    if (_currentFrameIndex >= _indices.size())
    {
        // 第一帧，设置为0
        _currentFrameIndex = 0;
    }
    else
    {
        ++_currentFrameIndex;
        if (_currentFrameIndex > _indices.size() - 1) _currentFrameIndex = 0;  // 循环

        // 移动历史帧的索引
        for (size_t i = _indices.size() - 1; i >= 1; --i)
        {
            _indices[i] = _indices[i - 1];
        }
    }

    // 设置当前帧的索引
    _indices[0] = _currentFrameIndex;
}

// 获取相对帧索引
// 获取相对于当前帧的索引
// relativeFrameIndex: 相对帧索引（0=当前帧，1=上一帧，以此类推）
// 返回值：实际帧索引
size_t RecordAndSubmitTask::index(size_t relativeFrameIndex) const
{
    return relativeFrameIndex < _indices.size() ? _indices[relativeFrameIndex] : _indices.size();
}

// 获取围栏
// fence()和fence(0)返回当前正在渲染的帧的围栏，fence(1)返回上一帧的围栏，以此类推
// relativeFrameIndex: 相对帧索引
// 返回值：围栏对象，如果索引无效则返回nullptr
ref_ptr<Fence> RecordAndSubmitTask::fence(size_t relativeFrameIndex)
{
    size_t i = index(relativeFrameIndex);
    return i < _fences.size() ? _fences[i] : nullptr;
}

// 提交任务
// 执行完整的记录和提交流程
// frameStamp: 帧戳对象
// 返回值：Vulkan操作结果
VkResult RecordAndSubmitTask::submit(ref_ptr<FrameStamp> frameStamp)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "RecordAndSubmitTask submit", COLOR_RECORD);

    //info("\nRecordAndSubmitTask::submit()");

    // 开始任务（等待围栏等）
    if (VkResult result = start(); result != VK_SUCCESS) return result;

    // 执行记录遍历前的数据传输
    if (transferTask)
    {
        if (auto transfer = transferTask->transferData(TransferTask::TRANSFER_BEFORE_RECORD_TRAVERSAL); transfer.result == VK_SUCCESS)
        {
            if (transfer.dataTransferredSemaphore)
            {
                //info("    adding early transfer dataTransferredSemaphore ", transfer.dataTransferredSemaphore);
                earlyDataTransferredSemaphore = transfer.dataTransferredSemaphore;
            }
        }
        else
        {
            return transfer.result;
        }
    }

    // 创建已记录的命令缓冲区集合
    auto recordedCommandBuffers = RecordedCommandBuffers::create();

    // 记录命令缓冲区
    if (VkResult result = record(recordedCommandBuffers, frameStamp); result != VK_SUCCESS) return result;

    // 完成提交
    return finish(recordedCommandBuffers);
}

// 开始任务
// 等待当前帧的围栏，准备开始记录
// 返回值：Vulkan操作结果
VkResult RecordAndSubmitTask::start()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "RecordAndSubmitTask start", COLOR_RECORD);

    // 重置信号量
    earlyDataTransferredSemaphore.reset();
    lateDataTransferredSemaphore.reset();

    // 获取当前帧的围栏
    auto current_fence = fence();
    // 如果有依赖，等待围栏
    if (current_fence->hasDependencies())
    {
        //info("RecordAndSubmitTask::start() waiting on fence ", current_fence, ", ", current_fence->status(), ", current_fence->hasDependencies() = ", current_fence->hasDependencies());

        uint64_t timeout = std::numeric_limits<uint64_t>::max();
        if (VkResult result = current_fence->wait(timeout); result != VK_SUCCESS) return result;

        current_fence->resetFenceAndDependencies();  // 重置围栏和依赖

        //info("after RecordAndSubmitTask::start() waited on fence ", current_fence, ", ", current_fence->status(), ", current_fence->hasDependencies() = ", current_fence->hasDependencies());
    }
    else
    {
        //info("RecordAndSubmitTask::start() initial fence ", current_fence, ", ", current_fence->status(), ", current_fence->hasDependencies() = ", current_fence->hasDependencies());
    }

    return VK_SUCCESS;
}

// 记录命令缓冲区
// 遍历所有命令图并记录命令
// recordedCommandBuffers: 已记录的命令缓冲区集合
// frameStamp: 帧戳对象
// 返回值：Vulkan操作结果
VkResult RecordAndSubmitTask::record(ref_ptr<RecordedCommandBuffers> recordedCommandBuffers, ref_ptr<FrameStamp> frameStamp)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "RecordAndSubmitTask record", COLOR_RECORD);

    // 遍历所有命令图并记录命令
    for (auto& commandGraph : commandGraphs)
    {
        commandGraph->record(recordedCommandBuffers, frameStamp, databasePager);
    }

    return VK_SUCCESS;
}

// 完成任务
// 执行记录遍历后的数据传输，然后提交命令缓冲区到队列
// recordedCommandBuffers: 已记录的命令缓冲区集合
// 返回值：Vulkan操作结果
VkResult RecordAndSubmitTask::finish(ref_ptr<RecordedCommandBuffers> recordedCommandBuffers)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "RecordAndSubmitTask finish", COLOR_RECORD);

    //info("RecordAndSubmitTask::finish()");

    auto current_fence = fence();  // 获取当前帧的围栏

    // 执行记录遍历后的数据传输
    if (transferTask)
    {
        auto transfer = transferTask->transferData(TransferTask::TRANSFER_AFTER_RECORD_TRAVERSAL);

        if (transfer.result == VK_SUCCESS)
        {
            if (transfer.dataTransferredSemaphore)
            {
                //info("    adding late transfer dataTransferredSemaphore ", transfer.dataTransferredSemaphore);
                lateDataTransferredSemaphore = transfer.dataTransferredSemaphore;
            }
        }
        else
        {
            return transfer.result;
        }
    }

    // 如果没有命令缓冲区需要提交，提前返回
    if (recordedCommandBuffers->empty())
    {
        // 分配传输完成信号量
        if (earlyDataTransferredSemaphore) transferTask->assignTransferConsumedCompletedSemaphore(TransferTask::TRANSFER_BEFORE_RECORD_TRAVERSAL, earlyDataTransferredSemaphore);
        if (lateDataTransferredSemaphore) transferTask->assignTransferConsumedCompletedSemaphore(TransferTask::TRANSFER_AFTER_RECORD_TRAVERSAL, lateDataTransferredSemaphore);

        // 没有工作要做，提前返回
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 睡眠1/60秒
        return VK_SUCCESS;
    }

    // 将VSG命令缓冲区转换为Vulkan句柄，并添加到围栏的依赖命令缓冲区列表
    std::vector<VkCommandBuffer> vk_commandBuffers;
    std::vector<VkSemaphore> vk_waitSemaphores;
    std::vector<VkPipelineStageFlags> vk_waitStages;
    std::vector<VkSemaphore> vk_signalSemaphores;

    // 转换VSG命令缓冲区为Vulkan句柄，并添加到围栏的依赖命令缓冲区列表
    auto buffers = recordedCommandBuffers->buffers();
    for (auto& commandBuffer : buffers)
    {
        // 只添加主命令缓冲区
        if (commandBuffer->level() == VK_COMMAND_BUFFER_LEVEL_PRIMARY) vk_commandBuffers.push_back(*commandBuffer);

        // 添加到围栏的依赖命令缓冲区列表
        current_fence->dependentCommandBuffers().emplace_back(commandBuffer);
    }

    // 添加早期数据传输信号量
    if (earlyDataTransferredSemaphore)
    {
        vk_waitSemaphores.emplace_back(*earlyDataTransferredSemaphore);
        vk_waitStages.emplace_back(earlyDataTransferredSemaphore->pipelineStageFlags());
    }
    // 添加晚期数据传输信号量
    if (lateDataTransferredSemaphore)
    {
        vk_waitSemaphores.emplace_back(*lateDataTransferredSemaphore);
        vk_waitStages.emplace_back(lateDataTransferredSemaphore->pipelineStageFlags());
    }

    // 分配传输完成信号量
    if (earlyDataTransferredSemaphore) transferTask->assignTransferConsumedCompletedSemaphore(TransferTask::TRANSFER_BEFORE_RECORD_TRAVERSAL, earlyTransferConsumerCompletedSemaphore);
    if (lateDataTransferredSemaphore) transferTask->assignTransferConsumedCompletedSemaphore(TransferTask::TRANSFER_AFTER_RECORD_TRAVERSAL, lateTransferConsumerCompletedSemaphore);

    // 清空围栏的依赖信号量
    current_fence->dependentSemaphores().clear();

    // 添加窗口的图像可用和渲染完成信号量
    for (auto& window : windows)
    {
        auto imageIndex = window->imageIndex();
        if (imageIndex >= window->numFrames()) continue;

        auto& frame = window->frame(imageIndex);

        // 添加图像可用信号量（等待）
        vk_waitSemaphores.emplace_back(frame.imageAvailableSemaphore->vk());
        vk_waitStages.emplace_back(frame.imageAvailableSemaphore->pipelineStageFlags());

        // 添加渲染完成信号量（发出）
        vk_signalSemaphores.emplace_back(frame.renderFinishedSemaphore->vk());
        current_fence->dependentSemaphores().push_back(frame.renderFinishedSemaphore);
    }

    // 添加等待信号量
    for (auto& semaphore : waitSemaphores)
    {
        vk_waitSemaphores.emplace_back(semaphore->vk());
        vk_waitStages.emplace_back(semaphore->pipelineStageFlags());
    }

    // 添加发出信号量
    current_fence->dependentSemaphores() = signalSemaphores;
    for (auto& semaphore : signalSemaphores)
    {
        vk_signalSemaphores.emplace_back(semaphore->vk());
        current_fence->dependentSemaphores().push_back(semaphore);
    }

    // 添加传输完成信号量
    if (earlyDataTransferredSemaphore)
    {
        vk_signalSemaphores.emplace_back(earlyTransferConsumerCompletedSemaphore->vk());
        current_fence->dependentSemaphores().push_back(earlyTransferConsumerCompletedSemaphore);
    }
    if (lateDataTransferredSemaphore)
    {
        vk_signalSemaphores.emplace_back(lateTransferConsumerCompletedSemaphore->vk());
        current_fence->dependentSemaphores().push_back(lateTransferConsumerCompletedSemaphore);
    }

    // 填充提交信息
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(vk_waitSemaphores.size());
    submitInfo.pWaitSemaphores = vk_waitSemaphores.data();
    submitInfo.pWaitDstStageMask = vk_waitStages.data();

    submitInfo.commandBufferCount = static_cast<uint32_t>(vk_commandBuffers.size());
    submitInfo.pCommandBuffers = vk_commandBuffers.data();

    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(vk_signalSemaphores.size());
    submitInfo.pSignalSemaphores = vk_signalSemaphores.data();

    // 提交到队列
    return queue->submit(submitInfo, current_fence);
}

// 分配性能分析工具
// 将性能分析工具分配给任务及其子组件
// in_instrumentation: 性能分析工具对象
void RecordAndSubmitTask::assignInstrumentation(ref_ptr<Instrumentation> in_instrumentation)
{
    instrumentation = in_instrumentation;

    // 分配给数据库分页器
    if (databasePager) databasePager->assignInstrumentation(instrumentation);
    // 分配给传输任务（线程安全共享）
    if (transferTask) transferTask->instrumentation = shareOrDuplicateForThreadSafety(instrumentation);

    // 分配给所有命令图
    for (auto cg : commandGraphs)
    {
        cg->instrumentation = shareOrDuplicateForThreadSafety(instrumentation);
        cg->getOrCreateRecordTraversal()->instrumentation = cg->instrumentation;
    }
}

// 更新任务
// 根据编译结果更新任务，包括动态数据、最大槽位、数据库分页器和分箱
// tasks: 记录和提交任务列表
// compileManager: 编译管理器
// compileResult: 编译结果
void vsg::updateTasks(RecordAndSubmitTasks& tasks, ref_ptr<CompileManager> compileManager, const CompileResult& compileResult)
{
    //info("vsg::updateTasks(RecordAndSubmitTasks& tasks..) compileResult.maxSlot = ", compileResult.maxSlot);
    // 分配动态数据到传输任务
    if (compileResult.dynamicData)
    {
        for (const auto& task : tasks)
        {
            if (task->transferTask)
            {
                task->transferTask->assign(compileResult.dynamicData);
            }
        }
    }

    // 如果需要，增加最大槽位
    for (const auto& task : tasks)
    {
        for (const auto& commandGraph : task->commandGraphs)
        {
            commandGraph->maxSlots.merge(compileResult.maxSlots);
        }
    }

    // 如果需要，分配数据库分页器
    if (compileResult.containsPagedLOD)
    {
        ref_ptr<DatabasePager> databasePager;
        // 查找现有的数据库分页器
        for (const auto& task : tasks)
        {
            if (task->databasePager && !databasePager) databasePager = task->databasePager;
        }

        // 如果没有，创建新的数据库分页器
        if (!databasePager)
        {
            databasePager = DatabasePager::create();
            for (const auto& task : tasks)
            {
                if (!task->databasePager)
                {
                    task->databasePager = databasePager;
                    task->databasePager->compileManager = compileManager;
                }
            }

            databasePager->start();  // 启动数据库分页器
        }
    }

    // 处理任何新的分箱需求
    for (auto& [const_view, viewDetails] : compileResult.views)
    {
        auto view = const_cast<View*>(const_view);
        for (auto& binNumber : viewDetails.indices)
        {
            // 检查分箱是否已存在
            bool binNumberMatched = false;
            for (const auto& bin : view->bins)
            {
                if (bin->binNumber == binNumber)
                {
                    binNumberMatched = true;
                }
            }
            // 如果不存在，创建新的分箱
            if (!binNumberMatched)
            {
                // 根据分箱编号确定排序顺序
                Bin::SortOrder sortOrder = (binNumber < 0) ? Bin::ASCENDING : ((binNumber == 0) ? Bin::NO_SORT : Bin::DESCENDING);
                view->bins.push_back(Bin::create(binNumber, sortOrder));
            }
        }
    }
}
