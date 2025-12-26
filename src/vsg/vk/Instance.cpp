/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/io/Logger.h>
#include <vsg/vk/DeviceExtensions.h>
#include <vsg/vk/Instance.h>
#include <vsg/vk/PhysicalDevice.h>

#include <algorithm>
#include <cstring>
#include <set>

using namespace vsg;

// 枚举实例扩展属性
// pLayerName: 层名称（可选，nullptr表示不指定层）
// 返回: 扩展属性列表
// 枚举Vulkan实例可用的扩展，用于检查系统支持的扩展
ExtensionProperties vsg::enumerateInstanceExtensionProperties(const char* pLayerName)
{
    uint32_t extCount = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(pLayerName, &extCount, nullptr);
    if (result != VK_SUCCESS)
    {
        error("vsg::enumerateInstanceExtensionProperties(...) failed, could not get extension count from vkEnumerateInstanceExtensionProperties. VkResult = ", result);
        return {};
    }

    ExtensionProperties extensionProperties(extCount);
    result = vkEnumerateInstanceExtensionProperties(pLayerName, &extCount, extensionProperties.data());
    if (result != VK_SUCCESS)
    {
        error("vsg::enumerateInstanceExtensionProperties(...) failed, could not get extension properties from vkEnumerateInstanceExtensionProperties. VkResult = ", result);
        return {};
    }
    return extensionProperties;
}

// 检查扩展是否支持
// extensionName: 扩展名称
// pLayerName: 层名称（可选）
// 返回: 如果扩展支持则返回true
// 检查指定的Vulkan实例扩展是否可用
bool vsg::isExtensionSupported(const char* extensionName, const char* pLayerName)
{
    auto extProps = enumerateInstanceExtensionProperties(pLayerName);
    auto compare = [&](const VkExtensionProperties& rhs) { return strcmp(extensionName, rhs.extensionName) == 0; };
    return std::find_if(extProps.begin(), extProps.end(), compare) != extProps.end();
}

// 枚举实例层属性
// 返回: 层属性列表
// 枚举Vulkan实例可用的验证层和调试层
InstanceLayerProperties vsg::enumerateInstanceLayerProperties()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    return availableLayers;
}

// 验证实例层名称
// names: 请求的层名称列表
// 返回: 验证后的层名称列表（只包含可用的层）
// 验证请求的Vulkan实例层是否可用，过滤掉不可用的层
Names vsg::validateInstanceLayerNames(const Names& names)
{
    if (names.empty()) return names;

    auto availableLayers = enumerateInstanceLayerProperties();

    using NameSet = std::set<std::string>;
    NameSet layerNames;
    for (const auto& layer : availableLayers)
    {
        debug("layer=", layer.layerName);
        if (layer.layerName[0] != 0) layerNames.insert(layer.layerName);
    }

    Names validatedNames;
    validatedNames.reserve(names.size());
    for (const auto& requestedName : names)
    {
        if (layerNames.count(requestedName) != 0)
        {
            debug("valid requested layer : ", requestedName);
            validatedNames.push_back(requestedName);
        }
        else
        {
            warn("requested invalid layer : ", requestedName);
        }
    }

    return validatedNames;
}

// 调试工具消息回调函数
// messageSeverity: 消息严重程度
// pCallbackData: 回调数据（包含消息内容）
// 返回: VK_FALSE（表示不中断执行）
// Vulkan调试工具的回调函数，将Vulkan的调试消息转换为VSG日志消息
static VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/)
{
    vsg::Logger::Level level = vsg::Logger::LOGGER_INFO;
    if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
        level = vsg::Logger::LOGGER_ERROR;
    else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
        level = vsg::Logger::LOGGER_WARN;
    else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
        level = vsg::Logger::LOGGER_INFO;
    else if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0)
        level = vsg::Logger::LOGGER_DEBUG;

    vsg::log(level, "[Vulkan] ", pCallbackData->pMessage);

    return VK_FALSE;
}

// 构造函数：创建Vulkan实例对象
// instanceExtensions: 要启用的实例扩展名称列表
// layers: 要启用的验证层名称列表
// vulkanApiVersion: Vulkan API版本
// allocator: 内存分配器（可选）
// Vulkan实例是Vulkan应用程序的入口点，管理全局状态和物理设备枚举
Instance::Instance(Names instanceExtensions, Names layers, uint32_t vulkanApiVersion, AllocationCallbacks* allocator) :
    apiVersion(vulkanApiVersion)
{
    // 应用程序信息
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanSceneGraph application";
    appInfo.pEngineName = "VulkanSceneGraph";
    appInfo.engineVersion = VK_MAKE_VERSION(VSG_VERSION_MAJOR, VSG_VERSION_MINOR, VSG_VERSION_PATCH);
    appInfo.apiVersion = vulkanApiVersion;

    // 实例创建信息
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    createInfo.flags = 0;

#if defined(__APPLE__)
    // macOS需要端口枚举扩展
    if (vsg::isExtensionSupported(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
    {
        instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.empty() ? nullptr : instanceExtensions.data();

    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

    createInfo.pNext = nullptr;

    // 创建Vulkan实例
    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, allocator, &instance);
    if (result == VK_SUCCESS)
    {
        _instance = instance;
        _allocator = allocator;

        // 枚举所有物理设备
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // 为每个物理设备创建PhysicalDevice对象
        for (auto device : devices)
        {
            _physicalDevices.emplace_back(new PhysicalDevice(this, device));
        }
    }
    else
    {
        throw Exception{"Error: vsg::Instance::create(...) failed to create VkInstance.", result};
    }

    // 初始化扩展函数指针
    _extensions = InstanceExtensions::create(this);

    // 如果支持调试工具扩展，创建调试消息回调
    if (isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) && _extensions->vkCreateDebugUtilsMessengerEXT)
    {
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
        debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsMessengerCreateInfo.pfnUserCallback = debugUtilsMessengerCallback;
        result = _extensions->vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCreateInfo, nullptr, &_debugUtilsMessenger);
    }
}

// 析构函数：销毁Vulkan实例对象
// 清理物理设备列表、调试消息回调，然后销毁Vulkan实例
Instance::~Instance()
{
    _physicalDevices.clear();

    if (_instance)
    {
        // 销毁调试消息回调
        if (_debugUtilsMessenger != VK_NULL_HANDLE && _extensions->vkDestroyDebugUtilsMessengerEXT)
        {
            _extensions->vkDestroyDebugUtilsMessengerEXT(_instance, _debugUtilsMessenger, nullptr);
        }

        // 销毁Vulkan实例
        vkDestroyInstance(_instance, _allocator);
    }
}

ref_ptr<PhysicalDevice> Instance::getPhysicalDevice(VkQueueFlags queueFlags, const PhysicalDeviceTypes& deviceTypePreferences) const
{
    for (size_t i = 0; i <= deviceTypePreferences.size(); ++i)
    {
        for (auto& device : _physicalDevices)
        {
            if (i < deviceTypePreferences.size() && device->getProperties().deviceType != deviceTypePreferences[i]) continue;

            if (auto family = device->getQueueFamily(queueFlags); (family >= 0)) return device;
        }
    }
    return {};
}

ref_ptr<PhysicalDevice> Instance::getPhysicalDevice(VkQueueFlags queueFlags, Surface* surface, const PhysicalDeviceTypes& deviceTypePreferences) const
{
    for (size_t i = 0; i <= deviceTypePreferences.size(); ++i)
    {
        for (auto& device : _physicalDevices)
        {
            if (i < deviceTypePreferences.size() && device->getProperties().deviceType != deviceTypePreferences[i]) continue;

            if (auto [graphicsFamily, presentFamily] = device->getQueueFamily(queueFlags, surface); (graphicsFamily >= 0 && presentFamily >= 0)) return device;
        }
    }
    return {};
}

std::pair<ref_ptr<PhysicalDevice>, int> Instance::getPhysicalDeviceAndQueueFamily(VkQueueFlags queueFlags, const PhysicalDeviceTypes& deviceTypePreferences) const
{
    for (size_t i = 0; i <= deviceTypePreferences.size(); ++i)
    {
        for (auto& device : _physicalDevices)
        {
            if (i < deviceTypePreferences.size() && device->getProperties().deviceType != deviceTypePreferences[i]) continue;

            if (auto family = device->getQueueFamily(queueFlags); family >= 0) return {device, family};
        }
    }
    return {{}, -1};
}

std::tuple<ref_ptr<PhysicalDevice>, int, int> Instance::getPhysicalDeviceAndQueueFamily(VkQueueFlags queueFlags, Surface* surface, const PhysicalDeviceTypes& deviceTypePreferences) const
{
    for (size_t i = 0; i <= deviceTypePreferences.size(); ++i)
    {
        for (auto& device : _physicalDevices)
        {
            if (i < deviceTypePreferences.size() && device->getProperties().deviceType != deviceTypePreferences[i]) continue;

            if (auto [graphicsFamily, presentFamily] = device->getQueueFamily(queueFlags, surface); (graphicsFamily >= 0 && presentFamily >= 0))
            {
                return {device, graphicsFamily, presentFamily};
            }
        }
    }
    return {{}, -1, -1};
}
