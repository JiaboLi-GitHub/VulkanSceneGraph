/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/TransferTask.h>
#include <vsg/commands/CopyAndReleaseImage.h>
#include <vsg/commands/PipelineBarrier.h>
#include <vsg/io/Logger.h>
#include <vsg/utils/Instrumentation.h>

using namespace vsg;

// 构造函数：创建复制并释放图像命令
// optional_stagingMemoryBufferPools: 可选的临时内存缓冲区池（用于暂存数据传输）
// 复制并释放图像命令用于将数据从CPU复制到GPU图像，并在完成后释放临时资源
CopyAndReleaseImage::CopyAndReleaseImage(ref_ptr<MemoryBufferPools> optional_stagingMemoryBufferPools) :
    stagingMemoryBufferPools(optional_stagingMemoryBufferPools)
{
}

// 析构函数：销毁复制并释放图像命令
CopyAndReleaseImage::~CopyAndReleaseImage()
{
}

// 复制数据构造函数：创建复制数据对象
// src: 源缓冲区信息
// dest: 目标图像信息
// numMipMapLevels: Mipmap级别数量
// 从源数据中提取图像属性（布局、宽度、高度、深度）
CopyAndReleaseImage::CopyData::CopyData(ref_ptr<BufferInfo> src, ref_ptr<ImageInfo> dest, uint32_t numMipMapLevels) :
    source(src),
    destination(dest),
    mipLevels(numMipMapLevels)
{
    if (source->data)
    {
        layout = source->data->properties;
        width = source->data->width();
        height = source->data->height();
        depth = source->data->depth();
    }
}

// 添加复制数据到待处理列表
// cd: 复制数据对象
// 将复制操作添加到待处理列表，等待记录到命令缓冲区
void CopyAndReleaseImage::add(const CopyData& cd)
{
    std::scoped_lock lock(_mutex);
    _pending.push_back(cd);
}

// 添加复制操作（自动计算Mipmap级别）
// src: 源缓冲区信息
// dest: 目标图像信息
// 根据源数据和采样器自动计算Mipmap级别数量
void CopyAndReleaseImage::add(ref_ptr<BufferInfo> src, ref_ptr<ImageInfo> dest)
{
    add(CopyData(src, dest, vsg::computeNumMipMapLevels(src->data, dest->sampler)));
}

// 添加复制操作（指定Mipmap级别）
// src: 源缓冲区信息
// dest: 目标图像信息
// numMipMapLevels: Mipmap级别数量
void CopyAndReleaseImage::add(ref_ptr<BufferInfo> src, ref_ptr<ImageInfo> dest, uint32_t numMipMapLevels)
{
    add(CopyData(src, dest, numMipMapLevels));
}

// 复制数据到目标图像（自动计算Mipmap级别）
// data: 要复制的数据对象
// dest: 目标图像信息
// 根据数据和采样器自动计算Mipmap级别数量
void CopyAndReleaseImage::copy(ref_ptr<Data> data, ref_ptr<ImageInfo> dest)
{
    copy(data, dest, vsg::computeNumMipMapLevels(data, dest->sampler));
}

// 复制数据到目标图像（指定Mipmap级别）
// data: 要复制的数据对象
// dest: 目标图像信息
// numMipMapLevels: Mipmap级别数量
// 如果源格式和目标格式相同或兼容，直接复制；否则进行格式转换
void CopyAndReleaseImage::copy(ref_ptr<Data> data, ref_ptr<ImageInfo> dest, uint32_t numMipMapLevels)
{
    if (!data) return;
    if (!stagingMemoryBufferPools) return;

    VkFormat sourceFormat = data->properties.format;
    VkFormat targetFormat = dest->imageView->format;

    // 如果格式相同，直接复制
    if (sourceFormat == targetFormat)
    {
        _copyDirectly(data, dest, numMipMapLevels);
        return;
    }

    auto sourceTraits = getFormatTraits(sourceFormat);
    auto targetTraits = getFormatTraits(targetFormat);

    // 如果格式大小一致，假设格式兼容
    bool formatsCompatible = sourceTraits.size == targetTraits.size;
    if (formatsCompatible)
    {
        _copyDirectly(data, dest, numMipMapLevels);
    }
    else
    {
        // 格式不兼容，需要转换：分配临时缓冲区并转换数据
        VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        VkDeviceSize imageTotalSize = targetTraits.size * data->valueCount();
        VkDeviceSize alignment = std::max(VkDeviceSize(4), VkDeviceSize(targetTraits.size));

        auto stagingBufferInfo = stagingMemoryBufferPools->reserveBuffer(imageTotalSize, alignment, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, memoryPropertyFlags);

        auto deviceID = stagingMemoryBufferPools->device->deviceID;
        ref_ptr<Buffer> imageStagingBuffer(stagingBufferInfo->buffer);
        ref_ptr<DeviceMemory> imageStagingMemory(imageStagingBuffer->getDeviceMemory(deviceID));

        if (!imageStagingMemory) return;

        // 创建复制数据对象，设置目标格式属性
        vsg::CopyAndReleaseImage::CopyData cd(stagingBufferInfo, dest, numMipMapLevels);
        cd.width = data->width();
        cd.height = data->height();
        cd.depth = data->depth();
        cd.layout.format = targetFormat;
        cd.layout.stride = targetTraits.size;

        // 设置目标类型的默认值（用于填充缺失的通道）
        const uint8_t* default_ptr = targetTraits.defaultValue;
        uint32_t bytesFromSource = sourceTraits.size;
        uint32_t bytesToTarget = targetTraits.size;

        // 复制并转换数据
        using value_type = uint8_t;
        const value_type* src_ptr = reinterpret_cast<const value_type*>(data->dataPointer());

        void* buffer_data;
        imageStagingMemory->map(imageStagingBuffer->getMemoryOffset(deviceID) + stagingBufferInfo->offset, imageTotalSize, 0, &buffer_data);
        value_type* dest_ptr = reinterpret_cast<value_type*>(buffer_data);

        // 逐元素转换：复制源数据，然后用默认值填充剩余字节
        size_t valueCount = data->valueCount();
        for (size_t i = 0; i < valueCount; ++i)
        {
            uint32_t s = 0;
            // 复制源数据
            for (; s < bytesFromSource; ++s)
            {
                (*dest_ptr++) = *(src_ptr++);
            }

            // 用默认值填充剩余字节
            const value_type* src_default = default_ptr;
            for (; s < bytesToTarget; ++s)
            {
                (*dest_ptr++) = *(src_default++);
            }
        }

        imageStagingMemory->unmap();

        add(cd);
    }
}

// 直接复制数据到目标图像（格式相同或兼容）
// data: 要复制的数据对象
// dest: 目标图像信息
// numMipMapLevels: Mipmap级别数量
// 创建临时缓冲区，将数据复制到临时缓冲区，然后添加到待复制列表
void CopyAndReleaseImage::_copyDirectly(ref_ptr<Data> data, ref_ptr<ImageInfo> dest, uint32_t numMipMapLevels)
{
    VkDeviceSize imageTotalSize = data->dataSize();
    VkDeviceSize alignment = std::max(VkDeviceSize(4), VkDeviceSize(data->valueSize()));

    // 从临时内存缓冲区池分配临时缓冲区
    VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    auto stagingBufferInfo = stagingMemoryBufferPools->reserveBuffer(imageTotalSize, alignment, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, memoryPropertyFlags);
    stagingBufferInfo->data = data;

    auto deviceID = stagingMemoryBufferPools->device->deviceID;
    ref_ptr<Buffer> imageStagingBuffer(stagingBufferInfo->buffer);
    ref_ptr<DeviceMemory> imageStagingMemory(imageStagingBuffer->getDeviceMemory(deviceID));

    if (!imageStagingMemory) return;

    // 将数据复制到临时内存
    imageStagingMemory->copy(imageStagingBuffer->getMemoryOffset(deviceID) + stagingBufferInfo->offset, imageTotalSize, data->dataPointer());

    // 添加到待复制列表
    add(stagingBufferInfo, dest, numMipMapLevels);
}

// 记录复制数据到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 调用transferImageData函数，将缓冲区数据传输到图像
void CopyAndReleaseImage::CopyData::record(CommandBuffer& commandBuffer) const
{
    transferImageData(destination->imageView, destination->imageLayout, layout, width, height, depth, mipLevels, source->buffer, source->offset, commandBuffer.vk(), commandBuffer.getDevice());
}

// 记录复制并释放图像命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 将待处理的复制操作记录到命令缓冲区，并将它们移动到已完成列表以便后续释放
void CopyAndReleaseImage::record(CommandBuffer& commandBuffer) const
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
