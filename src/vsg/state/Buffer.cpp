/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/io/Logger.h>
#include <vsg/state/Buffer.h>
#include <vsg/vk/Context.h>

#define REPORT_STATS 1

using namespace vsg;

// 释放Vulkan数据
// 销毁Vulkan缓冲区并释放设备内存
void Buffer::VulkanData::release()
{
    if (buffer)
    {
        vkDestroyBuffer(*device, buffer, device->getAllocationCallbacks());
    }

    if (deviceMemory)
    {
        //deviceMemory->release(memoryOffset, memorySlots.totalMemorySize());
        deviceMemory->release(memoryOffset, size);
    }
}

// 构造函数：创建缓冲区对象
// in_size: 缓冲区大小（字节）
// in_usage: 缓冲区使用标志（如顶点缓冲区、索引缓冲区、统一缓冲区等）
// in_sharingMode: 共享模式（独占或并发）
// 缓冲区用于存储GPU可访问的数据（顶点、索引、统一缓冲区等）
Buffer::Buffer(VkDeviceSize in_size, VkBufferUsageFlags in_usage, VkSharingMode in_sharingMode) :
    flags(0),
    size(in_size),
    usage(in_usage),
    sharingMode(in_sharingMode),
    _memorySlots(in_size)
{
}

// 析构函数：销毁缓冲区对象
// 释放所有设备的Vulkan数据
Buffer::~Buffer()
{
#if REPORT_STATS
    debug("start of Buffer::~Buffer() ", this);
#endif

    for (auto& vd : _vulkanData) vd.release();

#if REPORT_STATS
    debug("end of Buffer::~Buffer() ", this);
#endif
}

// 获取内存需求
// deviceID: 设备ID
// 返回: Vulkan内存需求结构
// 查询缓冲区所需的内存大小、对齐和内存类型
VkMemoryRequirements Buffer::getMemoryRequirements(uint32_t deviceID) const
{
    const VulkanData& vd = _vulkanData[deviceID];
    if (!vd.buffer) return {};

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(*vd.device, vd.buffer, &memRequirements);
    return memRequirements;
}

// 绑定设备内存到缓冲区
// deviceMemory: 设备内存对象
// memoryOffset: 内存中的偏移量
// 返回: Vulkan结果代码
// 将设备内存绑定到缓冲区，使缓冲区可以使用该内存
VkResult Buffer::bind(DeviceMemory* deviceMemory, VkDeviceSize memoryOffset)
{
    VulkanData& vd = _vulkanData[deviceMemory->getDevice()->deviceID];

    if (vd.deviceMemory)
    {
        warn("Buffer::bind(", deviceMemory, ", ", memoryOffset, ") failed, buffer already bound to ", vd.deviceMemory);
        return VK_ERROR_UNKNOWN;
    }

    VkResult result = vkBindBufferMemory(*vd.device, vd.buffer, *deviceMemory, memoryOffset);
    if (result == VK_SUCCESS)
    {
        vd.deviceMemory = deviceMemory;
        vd.memoryOffset = memoryOffset;
        vd.size = size;
    }

    return result;
}

// 编译缓冲区（使用设备）
// device: Vulkan设备对象
// 返回: 如果已编译则返回false，否则返回true
// 创建Vulkan缓冲区对象
bool Buffer::compile(Device* device)
{
    VulkanData& vd = _vulkanData[device->deviceID];
    if (vd.buffer)
    {
        return false;
    }

    vd.device = device;
    vd.size = size;

    // 设置缓冲区创建信息
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.flags = flags;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = sharingMode;

    // 创建Vulkan缓冲区
    if (VkResult result = vkCreateBuffer(*device, &bufferInfo, device->getAllocationCallbacks(), &vd.buffer); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create VkBuffer.", result};
    }

    return true;
}

// 编译缓冲区（使用上下文）
// context: 编译上下文对象
// 返回: 如果已编译则返回false，否则返回true
// 委托给compile(Device*)方法
bool Buffer::compile(Context& context)
{
    return compile(context.device);
}

// 预留缓冲区中的内存槽
// in_size: 要预留的大小（字节）
// alignment: 对齐要求（字节）
// 返回: 可选的偏移量（如果预留成功）
// 从缓冲区的内存槽中预留指定大小和对齐的内存空间
MemorySlots::OptionalOffset Buffer::reserve(VkDeviceSize in_size, VkDeviceSize alignment)
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _memorySlots.reserve(in_size, alignment);
}

// 释放缓冲区中的内存槽
// offset: 要释放的偏移量
// in_size: 要释放的大小（字节）
// 释放之前预留的内存槽
void Buffer::release(VkDeviceSize offset, VkDeviceSize in_size)
{
    std::scoped_lock<std::mutex> lock(_mutex);
    _memorySlots.release(offset, in_size);
}

// 检查缓冲区是否已满
// 返回: 如果缓冲区已满则返回true
// 检查是否还有可用的内存槽
bool Buffer::full() const
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _memorySlots.full();
}

// 获取最大可用空间
// 返回: 最大可用空间大小（字节）
// 获取缓冲区中最大的连续可用空间
size_t Buffer::maximumAvailableSpace() const
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _memorySlots.maximumAvailableSpace();
}

// 获取总可用大小
// 返回: 总可用大小（字节）
// 获取缓冲区中所有可用空间的总和
size_t Buffer::totalAvailableSize() const
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _memorySlots.totalAvailableSize();
}

// 获取总预留大小
// 返回: 总预留大小（字节）
// 获取缓冲区中已预留空间的总和
size_t Buffer::totalReservedSize() const
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _memorySlots.totalReservedSize();
}

// 创建缓冲区并分配内存
// device: Vulkan设备对象
// size: 缓冲区大小（字节）
// usage: 缓冲区使用标志
// sharingMode: 共享模式
// memoryProperties: 内存属性标志
// pNextAllocInfo: 可选的分配信息扩展
// 返回: 已创建并绑定内存的缓冲区对象
// 创建缓冲区，分配设备内存，并将内存绑定到缓冲区
ref_ptr<Buffer> vsg::createBufferAndMemory(Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, VkMemoryPropertyFlags memoryProperties, void* pNextAllocInfo)
{
    auto buffer = vsg::Buffer::create(size, usage, sharingMode);
    buffer->compile(device);

    auto memRequirements = buffer->getMemoryRequirements(device->deviceID);
    auto memory = vsg::DeviceMemory::create(device, memRequirements, memoryProperties, pNextAllocInfo);

    buffer->bind(memory, 0);
    return buffer;
}
