/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/Event.h>
#include <vsg/core/Exception.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Event
//
// 构造函数：创建Vulkan事件对象
// device: Vulkan设备对象
// flags: 事件创建标志
// 事件用于在命令缓冲区之间进行细粒度的同步
Event::Event(Device* device, VkEventCreateFlags flags) :
    _device(device)
{
    VkEventCreateInfo eventCreateInfo;
    eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
    eventCreateInfo.pNext = nullptr;
    eventCreateInfo.flags = flags;

    if (VkResult result = vkCreateEvent(*device, &eventCreateInfo, nullptr, &_event); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create Event.", result};
    }
}

// 析构函数：销毁Vulkan事件对象
Event::~Event()
{
    vkDestroyEvent(*_device, _event, nullptr);
}

// 设置事件状态为已发出
// 将事件状态设置为已发出，等待该事件的命令可以继续执行
void Event::set()
{
    vkSetEvent(*_device, _event);
}

// 重置事件状态为未发出
// 将事件状态重置为未发出
void Event::reset()
{
    vkResetEvent(*_device, _event);
}

// 获取事件状态
// 返回: 事件状态（VK_EVENT_SET表示已发出，VK_EVENT_RESET表示未发出）
VkResult Event::status()
{
    return vkGetEventStatus(*_device, _event);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// SetEvent
//
// 构造函数：创建设置事件命令
// in_event: 要设置的事件对象
// in_stageMask: 管道阶段掩码（指定在哪个阶段设置事件）
// 设置事件命令用于在命令缓冲区中设置事件状态
SetEvent::SetEvent(ref_ptr<Event> in_event, VkPipelineStageFlags in_stageMask) :
    event(in_event),
    stageMask(in_stageMask)
{
}

// 析构函数：销毁设置事件命令
SetEvent::~SetEvent()
{
}

// 记录设置事件命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdSetEvent命令，在指定管道阶段设置事件
void SetEvent::record(CommandBuffer& commandBuffer) const
{
    vkCmdSetEvent(commandBuffer, *event, stageMask);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// ResetEvent - 重置事件命令
//
// 构造函数：创建重置事件命令
// in_event: 要重置的事件对象
// in_stageMask: 管道阶段掩码（指定在哪个阶段重置事件）
// 重置事件命令用于在命令缓冲区中重置事件状态
ResetEvent::ResetEvent(ref_ptr<Event> in_event, VkPipelineStageFlags in_stageMask) :
    event(in_event),
    stageMask(in_stageMask)
{
}

// 析构函数：销毁重置事件命令
ResetEvent::~ResetEvent()
{
}

// 记录重置事件命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdResetEvent命令，在指定管道阶段重置事件
void ResetEvent::record(CommandBuffer& commandBuffer) const
{
    vkCmdResetEvent(commandBuffer, *event, stageMask);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// WaitEvents - 等待事件命令
//
// 构造函数：创建等待事件命令（默认）
// 等待事件命令用于等待一个或多个事件被设置，然后执行内存屏障
WaitEvents::WaitEvents() :
    srcStageMask(0),
    dstStageMask(0)
{
}

// 析构函数：销毁等待事件命令
WaitEvents::~WaitEvents()
{
}

// 记录等待事件命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 从临时内存分配Vulkan结构，填充事件、内存屏障、缓冲区内存屏障和图像内存屏障，然后执行vkCmdWaitEvents命令
void WaitEvents::record(CommandBuffer& commandBuffer) const
{
    auto& scratchMemory = *(commandBuffer.scratchMemory);

    // 分配并填充事件数组
    auto vk_events = scratchMemory.allocate<VkEvent>(events.size());
    for (size_t i = 0; i < events.size(); ++i)
    {
        vk_events[i] = events[i]->vk();
    }

    // 分配并填充内存屏障
    auto vk_memoryBarriers = scratchMemory.allocate<VkMemoryBarrier>(memoryBarriers.size());
    for (size_t i = 0; i < memoryBarriers.size(); ++i)
    {
        memoryBarriers[i]->assign(commandBuffer, vk_memoryBarriers[i]);
    }

    // 分配并填充缓冲区内存屏障
    auto vk_bufferMemoryBarriers = scratchMemory.allocate<VkBufferMemoryBarrier>(bufferMemoryBarriers.size());
    for (size_t i = 0; i < bufferMemoryBarriers.size(); ++i)
    {
        bufferMemoryBarriers[i]->assign(commandBuffer, vk_bufferMemoryBarriers[i]);
    }

    // 分配并填充图像内存屏障
    auto vk_imageMemoryBarriers = scratchMemory.allocate<VkImageMemoryBarrier>(imageMemoryBarriers.size());
    for (size_t i = 0; i < imageMemoryBarriers.size(); ++i)
    {
        imageMemoryBarriers[i]->assign(commandBuffer, vk_imageMemoryBarriers[i]);
    }

    // 执行等待事件命令
    vkCmdWaitEvents(
        commandBuffer,
        static_cast<uint32_t>(events.size()),
        vk_events,
        srcStageMask,
        dstStageMask,
        static_cast<uint32_t>(memoryBarriers.size()),
        vk_memoryBarriers,
        static_cast<uint32_t>(bufferMemoryBarriers.size()),
        vk_bufferMemoryBarriers,
        static_cast<uint32_t>(imageMemoryBarriers.size()),
        vk_imageMemoryBarriers);

    // 释放临时内存
    scratchMemory.release();
}
