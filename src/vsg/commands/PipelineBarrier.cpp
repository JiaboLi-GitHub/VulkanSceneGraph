/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/PipelineBarrier.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// MemoryBarrier
//
// 分配内存屏障信息
// commandBuffer: 命令缓冲区对象
// info: 输出参数，用于填充Vulkan内存屏障结构
// 将内存屏障数据填充到Vulkan结构中，包括源访问掩码和目标访问掩码
void MemoryBarrier::assign(CommandBuffer& commandBuffer, VkMemoryBarrier& info) const
{
    info.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    info.pNext = next ? next->assign(commandBuffer) : nullptr;
    info.srcAccessMask = srcAccessMask;
    info.dstAccessMask = dstAccessMask;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// BufferMemoryBarrier - 缓冲区内存屏障
//
// 分配缓冲区内存屏障信息
// commandBuffer: 命令缓冲区对象
// info: 输出参数，用于填充Vulkan缓冲区内存屏障结构
// 将缓冲区内存屏障数据填充到Vulkan结构中，包括访问掩码、队列族索引、缓冲区和范围
void BufferMemoryBarrier::assign(CommandBuffer& commandBuffer, VkBufferMemoryBarrier& info) const
{
    info.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    info.pNext = next ? next->assign(commandBuffer) : nullptr;
    info.srcAccessMask = srcAccessMask;
    info.dstAccessMask = dstAccessMask;
    info.srcQueueFamilyIndex = srcQueueFamilyIndex; // Queue::queueFamilyIndex() 或 VK_QUEUE_FAMILY_IGNORED
    info.dstQueueFamilyIndex = dstQueueFamilyIndex; // Queue::queueFamilyIndex() 或 VK_QUEUE_FAMILY_IGNORED
    info.buffer = buffer.valid() ? buffer->vk(commandBuffer.deviceID) : VK_NULL_HANDLE;
    info.offset = offset;
    info.size = size;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// ImageMemoryBarrier - 图像内存屏障
//
// 分配图像内存屏障信息
// commandBuffer: 命令缓冲区对象
// info: 输出参数，用于填充Vulkan图像内存屏障结构
// 将图像内存屏障数据填充到Vulkan结构中，包括访问掩码、布局转换、队列族索引、图像和子资源范围
void ImageMemoryBarrier::assign(CommandBuffer& commandBuffer, VkImageMemoryBarrier& info) const
{
    info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    info.pNext = next ? next->assign(commandBuffer) : nullptr;
    info.srcAccessMask = srcAccessMask;
    info.dstAccessMask = dstAccessMask;
    info.oldLayout = oldLayout;
    info.newLayout = newLayout;
    info.srcQueueFamilyIndex = srcQueueFamilyIndex; // Queue::queueFamilyIndex() 或 VK_QUEUE_FAMILY_IGNORED
    info.dstQueueFamilyIndex = dstQueueFamilyIndex; // Queue::queueFamilyIndex() 或 VK_QUEUE_FAMILY_IGNORED
    info.image = image.valid() ? image->vk(commandBuffer.deviceID) : VK_NULL_HANDLE;
    info.subresourceRange = subresourceRange;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// SampleLocations - 采样位置
//
// 分配采样位置信息
// commandBuffer: 命令缓冲区对象
// 返回: 指向Vulkan采样位置信息结构的指针
// 从临时内存分配Vulkan采样位置信息结构并填充数据
void* SampleLocations::assign(CommandBuffer& commandBuffer) const
{
    auto info = commandBuffer.scratchMemory->allocate<VkSampleLocationsInfoEXT>(1);

    info->sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT;
    info->pNext = next ? next->assign(commandBuffer) : nullptr;
    info->sampleLocationsPerPixel = sampleLocationsPerPixel;
    info->sampleLocationGridSize = sampleLocationGridSize;
    info->sampleLocationsCount = static_cast<uint32_t>(sampleLocations.size());
    info->pSampleLocations = reinterpret_cast<const VkSampleLocationEXT*>(sampleLocations.data());

    return info;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// PipelineBarrier - 管道屏障
//
// 构造函数：创建管道屏障命令
// 管道屏障用于同步GPU操作，确保内存访问和资源状态转换的正确顺序
PipelineBarrier::PipelineBarrier()
{
}

// 析构函数：销毁管道屏障命令
PipelineBarrier::~PipelineBarrier()
{
}

// 记录管道屏障命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 从临时内存分配Vulkan屏障结构，填充所有内存屏障、缓冲区内存屏障和图像内存屏障，然后执行vkCmdPipelineBarrier命令
void PipelineBarrier::record(CommandBuffer& commandBuffer) const
{
    auto& scratchMemory = *(commandBuffer.scratchMemory);

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

    // 执行管道屏障命令
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        dependencyFlags,
        static_cast<uint32_t>(memoryBarriers.size()),
        vk_memoryBarriers,
        static_cast<uint32_t>(bufferMemoryBarriers.size()),
        vk_bufferMemoryBarriers,
        static_cast<uint32_t>(imageMemoryBarriers.size()),
        vk_imageMemoryBarriers);

    // 释放临时内存
    scratchMemory.release();
}
