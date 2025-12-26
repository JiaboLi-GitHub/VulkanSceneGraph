/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/vk/CommandBuffer.h>
#include <vsg/vk/CommandPool.h>

using namespace vsg;

// 构造函数：创建命令池对象
// device: 设备对象
// in_queueFamilyIndex: 队列族索引
// in_flags: 命令池创建标志
// 命令池用于分配命令缓冲区，必须与特定的队列族关联
CommandPool::CommandPool(Device* device, uint32_t in_queueFamilyIndex, VkCommandPoolCreateFlags in_flags) :
    queueFamilyIndex(in_queueFamilyIndex),
    flags(in_flags),
    _device(device)
{
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = flags;
    poolInfo.pNext = nullptr;

    if (VkResult result = vkCreateCommandPool(*device, &poolInfo, _device->getAllocationCallbacks(), &_commandPool); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create command pool.", result};
    }
}

// 析构函数：销毁命令池对象
// 销毁Vulkan命令池
CommandPool::~CommandPool()
{
    if (_commandPool)
    {
        vkDestroyCommandPool(*_device, _commandPool, _device->getAllocationCallbacks());
    }
}

// 分配命令缓冲区
// level: 命令缓冲区级别（主命令缓冲区或辅助命令缓冲区）
// 返回: 命令缓冲区对象
// 从命令池分配一个命令缓冲区
ref_ptr<CommandBuffer> CommandPool::allocate(VkCommandBufferLevel level)
{
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = _commandPool;
    allocateInfo.level = level;
    allocateInfo.commandBufferCount = 1;

    std::scoped_lock<std::mutex> lock(_mutex);
    VkCommandBuffer commandBuffer;
    if (VkResult result = vkAllocateCommandBuffers(*_device, &allocateInfo, &commandBuffer); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create command buffers.", result};
    }

    return ref_ptr<CommandBuffer>(new CommandBuffer(this, commandBuffer, level));
}

// 释放命令缓冲区
// commandBuffer: 要释放的命令缓冲区指针
// 将命令缓冲区返回到命令池
void CommandPool::free(CommandBuffer* commandBuffer)
{
    if (commandBuffer && commandBuffer->_commandBuffer)
    {
        std::scoped_lock<std::mutex> lock(_mutex);
        vkFreeCommandBuffers(*_device, _commandPool, 1, commandBuffer->data());
        commandBuffer->_commandBuffer = 0;
    }
}

// 重置命令池
// reset_flags: 重置标志
// 重置命令池，释放所有命令缓冲区（如果标志允许）
void CommandPool::reset(VkCommandPoolResetFlags reset_flags) const
{
    vkResetCommandPool(*_device, _commandPool, reset_flags);
}
