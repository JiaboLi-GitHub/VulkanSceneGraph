/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/io/Logger.h>
#include <vsg/utils/Profiler.h>
#include <vsg/vk/CommandBuffer.h>
#include <vsg/vk/State.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CommandBuffer - 命令缓冲区，用于记录和执行Vulkan命令
//
// 构造函数：创建命令缓冲区对象
// commandPool: 命令池对象
// commandBuffer: Vulkan命令缓冲区句柄
// level: 命令缓冲区级别（主命令缓冲区或辅助命令缓冲区）
// 命令缓冲区用于记录渲染命令，然后提交到队列执行
CommandBuffer::CommandBuffer(CommandPool* commandPool, VkCommandBuffer commandBuffer, VkCommandBufferLevel level) :
    deviceID(commandPool->getDevice()->deviceID),
    scratchMemory(ScratchMemory::create(4096)),
    _commandBuffer(commandBuffer),
    _level(level),
    _device(commandPool->getDevice()),
    _commandPool(commandPool),
    _currentPipelineLayout(VK_NULL_HANDLE),
    _currentPushConstantStageFlags(0)
{
}

// 析构函数：销毁命令缓冲区对象
// 将命令缓冲区返回到命令池
CommandBuffer::~CommandBuffer()
{
    if (_commandBuffer)
    {
        _commandPool->free(this);
    }
}

// 重置命令缓冲区
// flags: 重置标志
// 重置命令缓冲区状态，清除当前管道布局和推送常量阶段标志
void CommandBuffer::reset(VkCommandBufferResetFlags flags)
{
    _currentPipelineLayout = VK_NULL_HANDLE;
    _currentPushConstantStageFlags = 0;

    // 如果命令池支持重置命令缓冲区，则重置
    if ((_commandPool->flags & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) != 0)
    {
        vkResetCommandBuffer(_commandBuffer, flags);
    }
}

// 设置当前管道布局
// pipelineLayout: 管道布局对象
// 更新当前管道布局，如果布局改变则标记状态栈为脏（需要重新绑定描述符集）
void CommandBuffer::setCurrentPipelineLayout(const PipelineLayout* pipelineLayout)
{
    VkPipelineLayout newLayout = pipelineLayout->vk(deviceID);
    if (_currentPipelineLayout != newLayout)
    {
        // 必须假设所有描述符集都需要重新绑定
        state->dirtyStateStacks();

        _currentPipelineLayout = newLayout;
        // 更新推送常量阶段标志
        if (pipelineLayout->pushConstantRanges.empty())
            _currentPushConstantStageFlags = 0;
        else
            _currentPushConstantStageFlags = pipelineLayout->pushConstantRanges.front().stageFlags;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RecordedCommandBuffers - 已记录的命令缓冲区集合，支持按提交顺序组织
//
// 析构函数：销毁已记录的命令缓冲区集合对象
RecordedCommandBuffers::~RecordedCommandBuffers()
{
}

// 清空所有命令缓冲区
// 清空有序命令缓冲区映射和命令缓冲区列表
void RecordedCommandBuffers::clear()
{
    std::scoped_lock<std::mutex> lock(_mutex);
    _orderedCommandBuffers.clear();
    _commandBuffers.clear();
}

// 获取或创建指定提交顺序的已记录命令缓冲区
// submitOrder: 提交顺序
// 返回: 已记录的命令缓冲区对象
// 根据提交顺序获取或创建嵌套的命令缓冲区集合
ref_ptr<RecordedCommandBuffers> RecordedCommandBuffers::getOrCreateRecordedCommandBuffers(int submitOrder)
{
    auto& scb = _orderedCommandBuffers[submitOrder];
    if (!scb) scb = RecordedCommandBuffers::create();
    return scb;
}

// 添加命令缓冲区
// submitOrder: 提交顺序（0表示正常顺序，负数表示之前，正数表示之后）
// commandBuffer: 要添加的命令缓冲区
// 将命令缓冲区添加到集合中，根据提交顺序组织
void RecordedCommandBuffers::add(int submitOrder, ref_ptr<vsg::CommandBuffer> commandBuffer)
{
    std::scoped_lock<std::mutex> lock(_mutex);
    if (submitOrder == 0)
        _commandBuffers.push_back(commandBuffer);
    else
        getOrCreateRecordedCommandBuffers(submitOrder)->_commandBuffers.push_back(commandBuffer);
}

// 检查是否为空
// 返回: 如果所有命令缓冲区列表都为空则返回true
bool RecordedCommandBuffers::empty() const
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _commandBuffers.empty() && _orderedCommandBuffers.empty();
}

// 获取所有命令缓冲区（按提交顺序）
// 返回: 按提交顺序排列的命令缓冲区列表
// 合并所有命令缓冲区，按提交顺序排列（负数顺序 -> 0顺序 -> 正数顺序）
CommandBuffers RecordedCommandBuffers::buffers() const
{
    std::scoped_lock<std::mutex> lock(_mutex);

    if (_orderedCommandBuffers.empty()) return _commandBuffers;

    // 找到0位置的迭代器
    auto mid_itr = _orderedCommandBuffers.lower_bound(0);

    CommandBuffers buffers;
    // 先添加负数顺序的命令缓冲区
    for (auto itr = _orderedCommandBuffers.begin(); itr != mid_itr; ++itr)
    {
        auto nested_buffers = itr->second->buffers();
        buffers.insert(buffers.end(), nested_buffers.begin(), nested_buffers.end());
    }

    // 添加0顺序的命令缓冲区
    buffers.insert(buffers.end(), _commandBuffers.begin(), _commandBuffers.end());

    // 最后添加正数顺序的命令缓冲区
    for (auto itr = mid_itr; itr != _orderedCommandBuffers.end(); ++itr)
    {
        auto nested_buffers = itr->second->buffers();
        buffers.insert(buffers.end(), nested_buffers.begin(), nested_buffers.end());
    }

    return buffers;
}
