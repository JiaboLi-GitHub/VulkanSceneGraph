/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/io/Logger.h>
#include <vsg/vk/PhysicalDevice.h>

using namespace vsg;

// 构造函数：创建物理设备对象
// instance: Vulkan实例对象
// device: Vulkan物理设备句柄
// 物理设备代表系统中的GPU或其他Vulkan设备，包含设备属性和队列族信息
PhysicalDevice::PhysicalDevice(Instance* instance, VkPhysicalDevice device) :
    _device(device),
    _instance(instance)
{
    // 获取物理设备特性
    vkGetPhysicalDeviceFeatures(_device, &_features);

    // 获取物理设备属性
    vkGetPhysicalDeviceProperties(_device, &_properties);

    // 获取队列族属性
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, nullptr);

    _queueFamilies.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, _queueFamilies.data());

    /// 获取扩展函数指针
    instance->getProcAddr(_vkGetPhysicalDeviceFeatures2, "vkGetPhysicalDeviceFeatures2", "vkGetPhysicalDeviceFeatures2KHR");
    instance->getProcAddr(_vkGetPhysicalDeviceProperties2, "vkGetPhysicalDeviceProperties2", "vkGetPhysicalDeviceProperties2KHR");
}

// 析构函数：销毁物理设备对象
PhysicalDevice::~PhysicalDevice()
{
}

// 获取队列族索引
// queueFlags: 队列标志位（图形、计算、传输等）
// 返回: 队列族索引，如果未找到则返回-1
// 查找支持指定队列类型的队列族，优先返回完全匹配的队列族
int PhysicalDevice::getQueueFamily(VkQueueFlags queueFlags) const
{
    int bestFamily = -1;

    for (int i = 0; i < static_cast<int>(_queueFamilies.size()); ++i)
    {
        const auto& queueFamily = _queueFamilies[i];
        if ((queueFamily.queueFlags & queueFlags) == queueFlags)
        {
            // 检查是否完全匹配
            if (queueFamily.queueFlags == queueFlags)
            {
                return i;
            }

            if (bestFamily < 0) bestFamily = i;
        }
    }

    // 如果找不到传输队列，尝试使用图形队列（图形队列通常也支持传输）
    if (bestFamily < 0 && queueFlags == VK_QUEUE_TRANSFER_BIT)
    {
        return getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    }

    return bestFamily;
}

// 获取队列族索引（支持表面呈现）
// queueFlags: 队列标志位
// surface: 表面对象
// 返回: 队列族索引和呈现队列族索引的配对
// 查找支持指定队列类型且支持表面呈现的队列族
std::pair<int, int> PhysicalDevice::getQueueFamily(VkQueueFlags queueFlags, Surface* surface) const
{
    int queueFamily = -1;
    int presentFamily = -1;

    for (int i = 0; i < static_cast<int>(_queueFamilies.size()); ++i)
    {
        const auto& family = _queueFamilies[i];

        bool queueMatched = (family.queueFlags & queueFlags) == queueFlags;

        // 检查队列族是否支持表面呈现
        VkBool32 presentSupported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(_device, i, *surface, &presentSupported);

        // 如果队列匹配且支持呈现，返回同一个队列族
        if (queueMatched && presentSupported)
        {
            return {i, i};
        }

        if (queueMatched) queueFamily = i;
        if (presentSupported) presentFamily = i;
    }

    return {queueFamily, presentFamily};
}

// 枚举设备扩展属性
// pLayerName: 层名称（可选）
// 返回: 扩展属性列表
// 枚举物理设备支持的设备扩展
std::vector<VkExtensionProperties> PhysicalDevice::enumerateDeviceExtensionProperties(const char* pLayerName)
{
    uint32_t propertyCount;
    vkEnumerateDeviceExtensionProperties(_device, pLayerName, &propertyCount, nullptr);
    if (propertyCount == 0) return {};

    std::vector<VkExtensionProperties> extensionProperties(propertyCount);
    vkEnumerateDeviceExtensionProperties(_device, pLayerName, &propertyCount, extensionProperties.data());
    return extensionProperties;
}

// 检查设备扩展是否支持
// extensionName: 扩展名称
// 返回: 如果扩展支持则返回true
// 检查物理设备是否支持指定的设备扩展
bool PhysicalDevice::supportsDeviceExtension(const char* extensionName)
{
    auto extensionProperties = enumerateDeviceExtensionProperties();
    for (const auto& extensionProperty : extensionProperties)
    {
        if (std::strncmp(extensionProperty.extensionName, extensionName, VK_MAX_EXTENSION_NAME_SIZE) == 0)
            return true;
    }
    return false;
}
