/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/ExecuteCommands.h>

using namespace vsg;

// 构造函数：创建执行命令命令
// 执行命令命令用于在主命令缓冲区中执行多个辅助命令缓冲区
// 使用Latch来同步等待所有辅助命令缓冲区完成记录
ExecuteCommands::ExecuteCommands() :
    _latch(vsg::Latch::create(0))
{
}

// 析构函数：销毁执行命令命令
// 断开所有已连接的辅助命令图
ExecuteCommands::~ExecuteCommands()
{
    // 断开所有命令图
    for (auto& entry : _commandGraphsAndBuffers)
    {
        entry.cg->_disconnect(this);
    }
}

// 连接辅助命令图
// commandGraph: 要连接的辅助命令图
// 将辅助命令图添加到列表中，并建立连接关系
void ExecuteCommands::connect(ref_ptr<SecondaryCommandGraph> commandGraph)
{
    _commandGraphsAndBuffers.push_back(CommandGraphAndBuffer{commandGraph, {}});
    commandGraph->_connect(this);
}

// 重置执行命令命令
// 重置Latch计数，清空所有命令缓冲区引用，准备新一帧的记录
void ExecuteCommands::reset()
{
    std::scoped_lock lock(_mutex);

    // 设置Latch计数为命令图数量，等待所有命令图完成
    _latch->set(static_cast<int>(_commandGraphsAndBuffers.size()));

    // 清空所有命令缓冲区引用
    for (auto& entry : _commandGraphsAndBuffers)
    {
        entry.cb = {};
    }
}

// 命令图完成回调
// commandGraph: 完成记录的辅助命令图
// commandBuffer: 记录完成的命令缓冲区
// 当辅助命令图完成记录时调用，保存命令缓冲区引用并减少Latch计数
void ExecuteCommands::completed(const SecondaryCommandGraph& commandGraph, ref_ptr<CommandBuffer> commandBuffer)
{
    if (commandBuffer)
    {
        std::scoped_lock lock(_mutex);

        // 找到对应的命令图并保存命令缓冲区
        for (auto& [cg, cb] : _commandGraphsAndBuffers)
        {
            if (cg == &commandGraph)
            {
                cb = commandBuffer;
                break;
            }
        }
    }

    // 减少Latch计数
    _latch->count_down();
}

// 记录执行命令命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 等待所有辅助命令缓冲区完成记录，然后执行vkCmdExecuteCommands命令
void ExecuteCommands::record(CommandBuffer& commandBuffer) const
{
    // 等待所有辅助命令缓冲区完成记录
    _latch->wait();

    std::scoped_lock lock(_mutex);
    // 收集所有有效的命令缓冲区
    std::vector<VkCommandBuffer> vk_commandBuffers;
    for (auto& entry : _commandGraphsAndBuffers)
    {
        if (entry.cb) vk_commandBuffers.push_back(*entry.cb);
    }

    // 如果有有效的命令缓冲区，执行它们
    if (!vk_commandBuffers.empty())
    {
        vkCmdExecuteCommands(commandBuffer, static_cast<uint32_t>(vk_commandBuffers.size()), vk_commandBuffers.data());
    }
}
