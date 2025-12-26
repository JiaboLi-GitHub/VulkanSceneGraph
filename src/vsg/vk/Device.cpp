/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/Version.h>
#include <vsg/io/Logger.h>
#include <vsg/vk/DescriptorPools.h>
#include <vsg/vk/Device.h>
#include <vsg/vk/MemoryBufferPools.h>

#include <cstring>
#include <set>

using namespace vsg;

// 线程安全的容器，用于管理每个vsg::Device的设备ID
static std::mutex s_DeviceCountMutex;
static std::vector<bool> s_ActiveDevices;

// 获取唯一的设备ID
// 返回: 唯一的设备ID
// 为每个设备分配唯一的ID，用于多设备环境中的资源管理
static uint32_t getUniqueDeviceID()
{
    std::scoped_lock<std::mutex> guard(s_DeviceCountMutex);

    uint32_t deviceID = 0;
    // 查找第一个未使用的设备ID
    for (deviceID = 0; deviceID < static_cast<uint32_t>(s_ActiveDevices.size()); ++deviceID)
    {
        if (!s_ActiveDevices[deviceID])
        {
            s_ActiveDevices[deviceID] = true;
            return deviceID;
        }
    }

    // 如果没有未使用的ID，添加新的
    s_ActiveDevices.push_back(true);

    return deviceID;
}

// 释放设备ID
// deviceID: 要释放的设备ID
// 当设备销毁时释放其ID，以便重用
static void releaseDeviceID(uint32_t deviceID)
{
    std::scoped_lock<std::mutex> guard(s_DeviceCountMutex);
    s_ActiveDevices[deviceID] = false;
}

// 构造函数：创建逻辑设备对象
// physicalDevice: 物理设备对象
// queueSettings: 队列设置列表
// layers: 要启用的设备层名称列表
// deviceExtensions: 要启用的设备扩展名称列表
// deviceFeatures: 设备特性（可选）
// allocator: 内存分配器（可选）
// 逻辑设备是物理设备的抽象，用于创建队列、命令缓冲区等资源
Device::Device(PhysicalDevice* physicalDevice, const QueueSettings& queueSettings, Names layers, Names deviceExtensions, const DeviceFeatures* deviceFeatures, AllocationCallbacks* allocator) :
    deviceID(getUniqueDeviceID()),
    enabledExtensions(deviceExtensions),
    _instance(physicalDevice->getInstance()),
    _physicalDevice(physicalDevice),
    _allocator(allocator)
{
    // 检查设备数量限制
    if (deviceID >= VSG_MAX_DEVICES)
    {
        releaseDeviceID(deviceID);
        throw Exception{"Number of vsg:Device allocated exceeds number supported ", VSG_MAX_DEVICES};
    }

    const auto& queueFamilyProperties = physicalDevice->getQueueFamilyProperties();

    // 创建队列创建信息列表
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    // 为每个队列设置创建队列创建信息
    float queuePriority = 1.0f;
    for (auto& queueSetting : queueSettings)
    {
        if (queueSetting.queueFamilyIndex < 0) continue;

        // 检查队列族索引是否已经引用（Vulkan不支持重复的队列族）
        bool unique = true;
        for (const auto& existingInfo : queueCreateInfos)
        {
            if (existingInfo.queueFamilyIndex == static_cast<uint32_t>(queueSetting.queueFamilyIndex)) unique = false;
        }

        // Vulkan不支持非唯一的队列族，因此忽略此条目
        if (!unique) continue;

        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = static_cast<uint32_t>(queueSetting.queueFamilyIndex);

        // 如果指定了队列优先级，使用它们；否则使用默认优先级
        if (!queueSetting.queuePriorities.empty())
        {
            queueCreateInfo.queueCount = static_cast<uint32_t>(queueSetting.queuePriorities.size());
            queueCreateInfo.pQueuePriorities = queueSetting.queuePriorities.data();

            // 检查请求的队列数量是否超过物理设备支持的数量
            uint32_t supportedQueueCount = queueFamilyProperties[queueSetting.queueFamilyIndex].queueCount;
            if (queueCreateInfo.queueCount > supportedQueueCount)
            {
                queueCreateInfo.queueCount = supportedQueueCount;
                debug("Device::Device() creation failed to create requested queueCount.");
            }
        }
        else
        {
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
        }

        queueCreateInfo.pNext = nullptr;
        queueCreateInfos.push_back(queueCreateInfo);
    }

#if defined(__APPLE__)
    // macOS要求如果物理设备支持"VK_KHR_portability_subset"扩展，则必须请求它
    if (_physicalDevice->supportsDeviceExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    // 创建设备创建信息
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.empty() ? nullptr : queueCreateInfos.data();

    createInfo.pEnabledFeatures = nullptr;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();

    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

    createInfo.pNext = deviceFeatures ? deviceFeatures->data() : nullptr;

    // 创建逻辑设备
    VkResult result = vkCreateDevice(*physicalDevice, &createInfo, allocator, &_device);
    if (result != VK_SUCCESS)
    {
        releaseDeviceID(deviceID);
        throw Exception{"Error: vsg::Device::create(...) failed to create logical device.", result};
    }

    // 分配请求的队列
    for (auto queueInfo : queueCreateInfos)
    {
        for (uint32_t queueIndex = 0; queueIndex < queueInfo.queueCount; ++queueIndex)
        {
            VkQueue vk_queue;
            vkGetDeviceQueue(_device, queueInfo.queueFamilyIndex, queueIndex, &vk_queue);

            VkQueueFlags queueFlags = 0;

            if (queueInfo.queueFamilyIndex < queueFamilyProperties.size())
            {
                queueFlags = queueFamilyProperties[queueInfo.queueFamilyIndex].queueFlags;
            }
            else
            {
                warn("vsg::Device::Device(..) constructor unable to match queue family flags to PhysicalDevice queueFamilyProperties.");
            }

            ref_ptr<Queue> queue(new Queue(vk_queue, queueFlags, queueInfo.queueFamilyIndex, queueIndex));
            _queues.emplace_back(queue);
        }
    }

    // 初始化设备扩展函数指针
    _extensions = DeviceExtensions::create(this);
}

// 析构函数：销毁逻辑设备对象
// 销毁Vulkan逻辑设备并释放设备ID
Device::~Device()
{
    if (_device)
    {
        vkDestroyDevice(_device, _allocator);
    }

    releaseDeviceID(deviceID);
}

// 获取最大设备数量
// 返回: 支持的最大设备数量
// 返回VSG支持的最大设备数量常量
uint32_t Device::maxNumDevices()
{
    return VSG_MAX_DEVICES;
}

// 获取队列对象
// queueFamilyIndex: 队列族索引
// queueIndex: 队列索引
// 返回: 队列对象，如果未找到则返回空指针
// 根据队列族索引和队列索引查找队列对象，如果精确匹配失败则返回队列族中的第一个队列
ref_ptr<Queue> Device::getQueue(uint32_t queueFamilyIndex, uint32_t queueIndex)
{
    // 首先尝试精确匹配
    for (auto& queue : _queues)
    {
        if (queue->queueFamilyIndex() == queueFamilyIndex && queue->queueIndex() == queueIndex) return queue;
    }

    // 如果精确匹配失败，返回队列族中的第一个队列
    for (auto& queue : _queues)
    {
        if (queue->queueFamilyIndex() == queueFamilyIndex) return queue;
    }

    return {};
}

// 检查是否支持指定的API版本
// version: Vulkan API版本
// 返回: 如果实例和设备都支持该版本则返回true
// 检查Vulkan实例和物理设备是否都支持指定的API版本
bool Device::supportsApiVersion(uint32_t version) const
{
    return getInstance()->apiVersion >= version && _physicalDevice->getProperties().apiVersion >= version;
}

bool Device::supportsDeviceExtension(const char* extensionName) const
{
    auto compare = [&](const char* rhs) { return strcmp(extensionName, rhs) == 0; };
    return (std::find_if(enabledExtensions.begin(), enabledExtensions.end(), compare) != enabledExtensions.end());
}
