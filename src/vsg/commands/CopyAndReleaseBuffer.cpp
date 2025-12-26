/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/CopyAndReleaseBuffer.h>
#include <vsg/io/Logger.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 构造函数：创建复制并释放缓冲区命令
// optional_stagingMemoryBufferPools: 可选的临时内存缓冲区池（用于暂存数据传输）
// 复制并释放缓冲区命令用于将数据从CPU复制到GPU缓冲区，并在完成后释放临时资源
CopyAndReleaseBuffer::CopyAndReleaseBuffer(ref_ptr<MemoryBufferPools> optional_stagingMemoryBufferPools) :
    stagingMemoryBufferPools(optional_stagingMemoryBufferPools)
{
}

// 析构函数：销毁复制并释放缓冲区命令
CopyAndReleaseBuffer::~CopyAndReleaseBuffer()
{
}

// 复制数据到目标缓冲区
// data: 要复制的数据对象
// dest: 目标缓冲区信息
// 创建临时缓冲区，将数据复制到临时缓冲区，然后添加到待复制列表
void CopyAndReleaseBuffer::copy(ref_ptr<Data> data, ref_ptr<BufferInfo> dest)
{
    VkDeviceSize dataSize = data->dataSize();
    VkDeviceSize alignment = std::max(VkDeviceSize(4), VkDeviceSize(data->valueSize()));

    //debug("CopyAndReleaseImage::copyDirectly() dataSize = ", dataSize);

    // 从临时内存缓冲区池分配临时缓冲区
    VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    ref_ptr<BufferInfo> stagingBufferInfo = stagingMemoryBufferPools->reserveBuffer(dataSize, alignment, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, memoryPropertyFlags);
    stagingBufferInfo->data = data;

    //debug("stagingBufferInfo->buffer ", stagingBufferInfo->buffer.get(), ", ", stagingBufferInfo->offset, ", ", stagingBufferInfo->range, ")");

    auto deviceID = stagingMemoryBufferPools->device->deviceID;
    ref_ptr<Buffer> imageStagingBuffer(stagingBufferInfo->buffer);
    ref_ptr<DeviceMemory> stagingMemory(imageStagingBuffer->getDeviceMemory(deviceID));

    if (!stagingMemory) return;

    // 将数据复制到临时内存
    stagingMemory->copy(imageStagingBuffer->getMemoryOffset(deviceID) + stagingBufferInfo->offset, dataSize, data->dataPointer());

    // 添加到待复制列表
    add(stagingBufferInfo, dest);
}

// 添加复制操作到待处理列表
// src: 源缓冲区信息
// dest: 目标缓冲区信息
// 将复制操作添加到待处理列表，等待记录到命令缓冲区
void CopyAndReleaseBuffer::add(ref_ptr<BufferInfo> src, ref_ptr<BufferInfo> dest)
{
    std::scoped_lock lock(_mutex);
    _pending.push_back(CopyData{src, dest});
}

// 记录复制数据到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdCopyBuffer命令，从源缓冲区复制到目标缓冲区
void CopyAndReleaseBuffer::CopyData::record(CommandBuffer& commandBuffer) const
{
    //debug("CopyAndReleaseBuffer::CopyData::record(CommandBuffer& commandBuffer) source.offset = ", source->offset, ", ", destination->offset);
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = source->offset;
    copyRegion.dstOffset = destination->offset;
    copyRegion.size = source->range;
    vkCmdCopyBuffer(commandBuffer, source->buffer->vk(commandBuffer.deviceID), destination->buffer->vk(commandBuffer.deviceID), 1, &copyRegion);
}

// 记录复制并释放缓冲区命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 将待处理的复制操作记录到命令缓冲区，并将它们移动到已完成列表以便后续释放
void CopyAndReleaseBuffer::record(CommandBuffer& commandBuffer) const
{
    std::scoped_lock lock(_mutex);

    // 清空准备清除的列表（上一帧已完成的操作）
    _readyToClear.clear();

    // 将已完成的操作移动到准备清除列表
    _readyToClear.swap(_completed);

    // 记录所有待处理的复制操作
    for (const auto& copyData : _pending)
    {
        copyData.record(commandBuffer);
    }

    // 将待处理的操作移动到已完成列表（等待下一帧释放）
    _pending.swap(_completed);
}
