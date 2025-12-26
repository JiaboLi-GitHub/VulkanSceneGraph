/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/vk/DeviceMemory.h>

#include <atomic>
#include <cstring>

using namespace vsg;

#define DO_CHECK 0

static std::mutex s_DeviceMemoryListMutex;
static std::list<vsg::observer_ptr<DeviceMemory>> s_DeviceMemoryList;

// 获取活动的设备内存列表
// propertyFlags: 内存属性标志位
// 返回: 匹配指定属性的设备内存列表
// 从全局列表中获取所有活动的设备内存对象，过滤出匹配指定属性的内存
DeviceMemoryList vsg::getActiveDeviceMemoryList(VkMemoryPropertyFlagBits propertyFlags)
{
    std::scoped_lock<std::mutex> lock(s_DeviceMemoryListMutex);
    DeviceMemoryList dml;
    for (auto& dm : s_DeviceMemoryList)
    {
        auto dm_ref_ptr = dm.ref_ptr();
        if ((dm_ref_ptr->getMemoryPropertyFlags() & propertyFlags) != 0)
        {
            dml.push_back(dm_ref_ptr);
        }
    }
    return dml;
}

///////////////////////////////////////////////////////////////////////////////
//
// DeviceMemory - 设备内存，管理Vulkan设备内存分配
//
// 构造函数：创建设备内存对象
// device: 设备对象
// memRequirements: 内存需求（大小、对齐、内存类型位）
// properties: 内存属性标志（设备本地、主机可见等）
// pNextAllocInfo: 扩展分配信息（可选）
// 设备内存用于存储缓冲区、图像等资源的数据
DeviceMemory::DeviceMemory(Device* device, const VkMemoryRequirements& memRequirements, VkMemoryPropertyFlags properties, void* pNextAllocInfo) :
    _memoryRequirements(memRequirements),
    _properties(properties),
    _device(device),
    _memorySlots(memRequirements.size)
{
    uint32_t typeFilter = memRequirements.memoryTypeBits;

    // 查找要使用的内存类型
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(*(device->getPhysicalDevice()), &memProperties);
    uint32_t i;
    // 查找同时满足类型过滤器和属性要求的内存类型
    for (i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) break;
    }
    if (i >= memProperties.memoryTypeCount)
    {
        throw Exception{"Error: vsg::DeviceMemory::create(...) failed to create DeviceMemory, no usable memory type found.", VK_ERROR_FORMAT_NOT_SUPPORTED};
    }
    uint32_t memoryTypeIndex = i;

#if DO_CHECK
    if (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {
        static VkDeviceSize s_TotalDeviceMemoryAllocated = 0;
        s_TotalDeviceMemoryAllocated += memRequirements.size;
        debug("Device Local DeviceMemory::DeviceMemory() ", std::dec, memRequirements.size, ", ", memRequirements.alignment, ", ", memRequirements.memoryTypeBits, ",  s_TotalMemoryAllocated = ", s_TotalDeviceMemoryAllocated);
    }
    else
    {
        static VkDeviceSize s_TotalHostMemoryAllocated = 0;
        s_TotalHostMemoryAllocated += memRequirements.size;
        debug("Staging DeviceMemory::DeviceMemory()  ", std::dec, memRequirements.size, ", ", memRequirements.alignment, ", ", memRequirements.memoryTypeBits, ",  s_TotalMemoryAllocated = ", s_TotalHostMemoryAllocated);
    }
#endif

    // 分配设备内存
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memRequirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;
    allocateInfo.pNext = pNextAllocInfo;

    if (VkResult result = vkAllocateMemory(*device, &allocateInfo, _device->getAllocationCallbacks(), &_deviceMemory); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to allocate DeviceMemory.", result};
    }

    // 将设备内存添加到全局列表
    {
        std::scoped_lock<std::mutex> lock(s_DeviceMemoryListMutex);
        s_DeviceMemoryList.emplace_back(this);
        vsg::debug("DeviceMemory::DeviceMemory() added to s_DeviceMemoryList, s_DeviceMemoryList.size() = ", s_DeviceMemoryList.size());
    }
}

// 析构函数：销毁设备内存对象
// 释放Vulkan设备内存并从全局列表中移除
DeviceMemory::~DeviceMemory()
{
    if (_deviceMemory)
    {
#if DO_CHECK
        debug("DeviceMemory::~DeviceMemory() vkFreeMemory(*_device, ", _deviceMemory, ", _allocator);");
#endif

        vkFreeMemory(*_device, _deviceMemory, _device->getAllocationCallbacks());
    }

    {
        std::scoped_lock<std::mutex> lock(s_DeviceMemoryListMutex);
        auto itr = std::find(s_DeviceMemoryList.begin(), s_DeviceMemoryList.end(), this);
        if (itr != s_DeviceMemoryList.end())
        {
            s_DeviceMemoryList.erase(itr);
            vsg::debug("DeviceMemory::~DeviceMemory() removed from s_DeviceMemoryList, s_DeviceMemoryList.size() = ", s_DeviceMemoryList.size());
        }
        else
        {
            vsg::warn("DeviceMemory::~DeviceMemory() could not find in  s_DeviceMemoryList");
        }
    }
}

// 映射设备内存到主机地址空间
// offset: 内存偏移量
// size: 映射大小
// flags: 映射标志
// ppData: 输出参数，映射后的主机指针
// 返回: Vulkan结果
// 将设备内存映射到主机可访问的地址空间（仅适用于主机可见内存）
VkResult DeviceMemory::map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData)
{
    return vkMapMemory(*_device, _deviceMemory, offset, size, flags, ppData);
}

// 取消映射设备内存
// 取消设备内存的主机地址空间映射
void DeviceMemory::unmap()
{
    vkUnmapMemory(*_device, _deviceMemory);
}

// 复制数据到设备内存
// offset: 内存偏移量
// size: 数据大小
// src_data: 源数据指针
// 将数据从主机内存复制到设备内存（需要先映射）
void DeviceMemory::copy(VkDeviceSize offset, VkDeviceSize size, const void* src_data)
{
    // 是否应该检查缓冲区是否有足够的内存用于复制的数据？

    void* buffer_data;
    map(offset, size, 0, &buffer_data);

    std::memcpy(buffer_data, src_data, (size_t)size);

    unmap();
}

// 复制数据对象到设备内存
// offset: 内存偏移量
// data: 数据对象
// 将数据对象的内容复制到设备内存
void DeviceMemory::copy(VkDeviceSize offset, const Data* data)
{
    copy(offset, data->dataSize(), data->dataPointer());
}

// 预留内存槽
// size: 要预留的大小
// 返回: 可选偏移量（如果成功预留则包含偏移量）
// 从设备内存中预留指定大小的内存槽（考虑对齐要求）
MemorySlots::OptionalOffset DeviceMemory::reserve(VkDeviceSize size)
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _memorySlots.reserve(size, _memoryRequirements.alignment);
}

// 释放内存槽
// offset: 内存偏移量
// size: 要释放的大小
// 释放之前预留的内存槽
void DeviceMemory::release(VkDeviceSize offset, VkDeviceSize size)
{
    std::scoped_lock<std::mutex> lock(_mutex);
    _memorySlots.release(offset, size);
}

// 检查内存是否已满
// 返回: 如果内存已满则返回true
// 检查是否还有可用的内存槽
bool DeviceMemory::full() const
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _memorySlots.full();
}

// 获取最大可用空间
// 返回: 最大可用空间大小
// 获取当前最大的连续可用内存空间
VkDeviceSize DeviceMemory::maximumAvailableSpace() const
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _memorySlots.maximumAvailableSpace();
}

// 获取总可用大小
// 返回: 总可用大小
// 获取所有可用内存槽的总大小
size_t DeviceMemory::totalAvailableSize() const
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _memorySlots.totalAvailableSize();
}

// 获取总预留大小
// 返回: 总预留大小
// 获取所有已预留内存槽的总大小
size_t DeviceMemory::totalReservedSize() const
{
    std::scoped_lock<std::mutex> lock(_mutex);
    return _memorySlots.totalReservedSize();
}

// 获取总内存大小
// 返回: 总内存大小
// 获取设备内存的总大小
size_t DeviceMemory::totalMemorySize() const
{
    return _memorySlots.totalMemorySize();
}
