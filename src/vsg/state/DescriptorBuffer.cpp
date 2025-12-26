/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/io/Logger.h>
#include <vsg/state/DescriptorBuffer.h>
#include <vsg/vk/Context.h>

using namespace vsg;

/////////////////////////////////////////////////////////////////////////////////////////
//
// DescriptorBuffer
//
// 构造函数：创建描述符缓冲区对象（默认）
// 描述符缓冲区用于将缓冲区绑定到描述符集（统一缓冲区、存储缓冲区等）
// 默认类型为统一缓冲区
DescriptorBuffer::DescriptorBuffer() :
    Inherit(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
{
}

// 拷贝构造函数：从另一个描述符缓冲区对象创建新的描述符缓冲区对象
// rhs: 要拷贝的描述符缓冲区对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝缓冲区信息列表
DescriptorBuffer::DescriptorBuffer(const DescriptorBuffer& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    bufferInfoList(copyop(rhs.bufferInfoList))
{
}

// 构造函数：使用数据对象创建描述符缓冲区对象
// data: 数据对象（将被转换为BufferInfo）
// in_dstBinding: 目标绑定点索引
// in_dstArrayElement: 目标数组元素索引
// in_descriptorType: 描述符类型（统一缓冲区、存储缓冲区等）
DescriptorBuffer::DescriptorBuffer(ref_ptr<Data> data, uint32_t in_dstBinding, uint32_t in_dstArrayElement, VkDescriptorType in_descriptorType) :
    Inherit(in_dstBinding, in_dstArrayElement, in_descriptorType)
{
    if (data)
    {
        bufferInfoList.emplace_back(BufferInfo::create(data));
    }
}

// 构造函数：使用数据列表创建描述符缓冲区对象
// dataList: 数据对象列表
// in_dstBinding: 目标绑定点索引
// in_dstArrayElement: 目标数组元素索引
// in_descriptorType: 描述符类型
// 为每个数据对象创建BufferInfo
DescriptorBuffer::DescriptorBuffer(const DataList& dataList, uint32_t in_dstBinding, uint32_t in_dstArrayElement, VkDescriptorType in_descriptorType) :
    Inherit(in_dstBinding, in_dstArrayElement, in_descriptorType)
{
    bufferInfoList.reserve(dataList.size());
    for (auto& data : dataList)
    {
        bufferInfoList.emplace_back(BufferInfo::create(data));
    }
}

// 构造函数：使用缓冲区信息列表创建描述符缓冲区对象
// in_bufferInfoList: 缓冲区信息列表
// in_dstBinding: 目标绑定点索引
// in_dstArrayElement: 目标数组元素索引
// in_descriptorType: 描述符类型
DescriptorBuffer::DescriptorBuffer(const BufferInfoList& in_bufferInfoList, uint32_t in_dstBinding, uint32_t in_dstArrayElement, VkDescriptorType in_descriptorType) :
    Inherit(in_dstBinding, in_dstArrayElement, in_descriptorType),
    bufferInfoList(in_bufferInfoList)
{
}

// 析构函数：销毁描述符缓冲区对象
DescriptorBuffer::~DescriptorBuffer()
{
}

int DescriptorBuffer::compare(const Object& rhs_object) const
{
    int result = Descriptor::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    return compare_pointer_container(bufferInfoList, rhs.bufferInfoList);
}

void DescriptorBuffer::read(Input& input)
{
    Descriptor::read(input);

    bufferInfoList.clear();

    bufferInfoList.resize(input.readValue<uint32_t>("dataList"));
    for (auto& bufferInfo : bufferInfoList)
    {
        bufferInfo = vsg::BufferInfo::create();
        bufferInfo->buffer = nullptr;
        bufferInfo->offset = 0;
        bufferInfo->range = 0;
        input.readObject("data", bufferInfo->data);
    }
}

void DescriptorBuffer::write(Output& output) const
{
    Descriptor::write(output);

    output.writeValue<uint32_t>("dataList", bufferInfoList.size());
    for (const auto& bufferInfo : bufferInfoList)
    {
        output.writeObject("data", bufferInfo->data.get());
    }
}

// 编译描述符缓冲区
// context: 编译上下文对象
// 根据描述符类型确定缓冲区使用标志，如果需要则分配缓冲区，编译缓冲区并绑定内存，然后复制数据或分配给传输任务
void DescriptorBuffer::compile(Context& context)
{
    if (bufferInfoList.empty()) return;

    auto transferTask = context.transferTask.get();

    // 根据描述符类型确定缓冲区使用标志
    VkBufferUsageFlags bufferUsageFlags = 0;
    switch (descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        bufferUsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    default:
        break;
    }

    // 检查是否需要分配缓冲区
    bool requiresAssignmentOfBuffers = false;
    for (const auto& bufferInfo : bufferInfoList)
    {
        if (bufferInfo->buffer == nullptr) requiresAssignmentOfBuffers = true;
    }

    auto deviceID = context.deviceID;

    // 如果需要，分配缓冲区并预留槽位
    if (requiresAssignmentOfBuffers)
    {
        // 根据缓冲区类型确定对齐要求
        VkDeviceSize alignment = 4;
        const auto& limits = context.device->getPhysicalDevice()->getProperties().limits;
        if (bufferUsageFlags == VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
            alignment = limits.minUniformBufferOffsetAlignment;
        else if (bufferUsageFlags == VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
            alignment = limits.minStorageBufferOffsetAlignment;

        VkDeviceSize totalSize = 0;

        // 计算需要分配的BufferInfo的总大小
        {
            VkDeviceSize offset = 0;
            for (const auto& bufferInfo : bufferInfoList)
            {
                if (bufferInfo->data && !bufferInfo->buffer)
                {
                    totalSize = offset + bufferInfo->data->dataSize();
                    offset = (alignment == 1 || (totalSize % alignment) == 0) ? totalSize : ((totalSize / alignment) + 1) * alignment;
                    // 如果数据是动态的或使用传输任务，添加传输目标使用标志
                    if (bufferInfo->data->dynamic() || transferTask) bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                }
            }
        }

        // 如果需要，分配缓冲区并在其中为BufferInfo预留槽位
        if (totalSize > 0)
        {
            auto buffer = vsg::Buffer::create(totalSize, bufferUsageFlags, VK_SHARING_MODE_EXCLUSIVE);
            for (const auto& bufferInfo : bufferInfoList)
            {
                if (bufferInfo->data && !bufferInfo->buffer)
                {
                    auto [allocated, offset] = buffer->reserve(bufferInfo->data->dataSize(), alignment);
                    if (allocated)
                    {
                        bufferInfo->buffer = buffer;
                        bufferInfo->offset = offset;
                        bufferInfo->range = bufferInfo->data->dataSize();
                    }
                    else
                    {
                        warn("DescriptorBuffer::compile(..) unable to allocate bufferInfo within associated Buffer.");
                    }
                }
            }
        }
    }

    // 编译所有缓冲区并绑定内存
    for (auto& bufferInfo : bufferInfoList)
    {
        if (bufferInfo->buffer)
        {
            // 编译缓冲区
            if (bufferInfo->buffer->compile(context.device))
            {
                // 如果缓冲区未绑定内存，从设备内存缓冲区池分配内存
                if (bufferInfo->buffer->getDeviceMemory(deviceID) == nullptr)
                {
                    auto memRequirements = bufferInfo->buffer->getMemoryRequirements(deviceID);
                    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                    auto [deviceMemory, offset] = context.deviceMemoryBufferPools->reserveMemory(memRequirements, flags);
                    if (deviceMemory)
                    {
                        bufferInfo->buffer->bind(deviceMemory, offset);
                    }
                    else
                    {
                        throw Exception{"Error: DescriptorBuffer::compile(..) failed to allocate buffer from deviceMemoryBufferPools.", VK_ERROR_OUT_OF_DEVICE_MEMORY};
                    }
                }
            }

            // 如果没有传输任务且数据已修改，直接复制数据到缓冲区
            if (!transferTask && bufferInfo->data && bufferInfo->data->getModifiedCount(bufferInfo->copiedModifiedCounts[deviceID]))
            {
                bufferInfo->copyDataToBuffer(context.deviceID);
            }
        }
    }

    // 如果有传输任务，将缓冲区信息列表分配给传输任务
    if (transferTask) transferTask->assign(bufferInfoList);
}

// 将描述符缓冲区信息分配到Vulkan写入描述符集结构
// context: 编译上下文对象
// wds: 输出参数，用于填充Vulkan写入描述符集结构
// 从临时内存分配缓冲区信息数组，填充所有缓冲区信息（缓冲区句柄、偏移量、范围）
void DescriptorBuffer::assignTo(Context& context, VkWriteDescriptorSet& wds) const
{
    Descriptor::assignTo(context, wds);

    // 从临时内存分配缓冲区信息数组
    auto pBufferInfo = context.scratchMemory->allocate<VkDescriptorBufferInfo>(bufferInfoList.size());
    wds.descriptorCount = static_cast<uint32_t>(bufferInfoList.size());
    wds.pBufferInfo = pBufferInfo;

    // 从VSG转换为Vulkan格式
    for (size_t i = 0; i < bufferInfoList.size(); ++i)
    {
        auto& data = bufferInfoList[i];
        VkDescriptorBufferInfo& info = pBufferInfo[i];
        info.buffer = data->buffer->vk(context.deviceID);
        info.offset = data->offset;
        info.range = data->range;
    }
}

// 获取描述符数量
// 返回: 描述符数量（缓冲区信息列表的大小）
uint32_t DescriptorBuffer::getNumDescriptors() const
{
    return static_cast<uint32_t>(bufferInfoList.size());
}

// 将数据列表复制到缓冲区（所有设备）
// 为所有设备复制所有缓冲区信息的数据到缓冲区
void DescriptorBuffer::copyDataListToBuffers()
{
    for (auto& bufferInfo : bufferInfoList)
    {
        bufferInfo->copyDataToBuffer();
    }
}
