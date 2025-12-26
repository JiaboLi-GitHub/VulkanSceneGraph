/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/WindowTraits.h>
#include <vsg/io/Logger.h>
#include <vsg/utils/CommandLine.h>
#include <vsg/vk/vulkan.h>

#include <iostream>

using namespace vsg;

// WindowTraits类的默认构造函数
// 创建窗口特性对象，使用默认值
WindowTraits::WindowTraits()
{
    defaults();  // 设置默认值
}

// WindowTraits类的构造函数（从命令行参数）
// 从命令行参数解析窗口特性
// arguments: 命令行参数对象
WindowTraits::WindowTraits(CommandLine& arguments)
{
    defaults();  // 先设置默认值

    // 如果指定了--args，打印所有参数
    if (arguments.read("--args")) std::cout << arguments << std::endl;

    // 从参数中提取窗口标题
    windowTitle = vsg::make_string(arguments);
    // 读取调试层选项
    debugLayer = arguments.read({"--debug", "-d"});
    // 读取API转储层选项
    apiDumpLayer = arguments.read({"--api", "-a"});
    // 读取同步层选项
    synchronizationLayer = arguments.read("--sync");

    // 读取交换链缓冲区数量
    if (arguments.read("--double-buffer")) swapchainPreferences.imageCount = 2;
    if (arguments.read("--triple-buffer")) swapchainPreferences.imageCount = 3; // 默认值
    // 读取呈现模式
    if (arguments.read("--IMMEDIATE")) { swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; }
    if (arguments.read("--FIFO")) swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

    // 读取窗口模式
    if (arguments.read({"--fullscreen", "--fs"})) fullscreen = true;
    if (arguments.read({"--window", "-w"}, width, height)) { fullscreen = false; }
    // 读取窗口装饰选项
    if (arguments.read({"--no-frame"})) decoration = false;
    if (arguments.read("--or")) overrideRedirect = true;

    // 读取深度格式
    if (arguments.read("--d32")) depthFormat = VK_FORMAT_D32_SFLOAT;
    // 读取颜色格式
    if (arguments.read("--sRGB")) swapchainPreferences.surfaceFormat = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    if (arguments.read("--RGB")) swapchainPreferences.surfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    // 读取屏幕和显示设置
    arguments.read("--screen", screenNum);
    arguments.read("--display", display);
    // 读取多重采样设置
    arguments.read("--samples", samples);

    // Lambda函数：设置设备类型偏好（将指定类型移到列表开头）
    auto setDevicePref = [&](const VkPhysicalDeviceType typeIn) {
        auto it = std::find(deviceTypePreferences.begin(), deviceTypePreferences.end(), typeIn);
        if (it != deviceTypePreferences.end()) deviceTypePreferences.erase(it);
        deviceTypePreferences.insert(deviceTypePreferences.begin(), typeIn);
    };
    // 读取设备类型偏好
    if (arguments.read("--prefer-integrated")) setDevicePref(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
    if (arguments.read("--prefer-discrete")) setDevicePref(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    if (arguments.read("--prefer-virtual")) setDevicePref(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);
    if (arguments.read("--prefer-cpu")) setDevicePref(VK_PHYSICAL_DEVICE_TYPE_CPU);
}

// WindowTraits类的拷贝构造函数
// 从另一个WindowTraits对象复制所有特性
// traits: 要复制的WindowTraits对象
// copyop: 拷贝操作选项
WindowTraits::WindowTraits(const WindowTraits& traits, const CopyOp& copyop) :
    Inherit(traits, copyop),
    x(traits.x),  // 窗口X位置
    y(traits.y),  // 窗口Y位置
    width(traits.width),  // 窗口宽度
    height(traits.height),  // 窗口高度
    fullscreen(traits.fullscreen),  // 全屏模式
    display(traits.display),  // 显示名称
    screenNum(traits.screenNum),  // 屏幕编号
    windowClass(traits.windowClass),  // 窗口类
    windowTitle(traits.windowTitle),  // 窗口标题
    decoration(traits.decoration),  // 窗口装饰
    hdpi(traits.hdpi),  // 高DPI支持
    overrideRedirect(traits.overrideRedirect),  // 覆盖重定向
    vulkanVersion(traits.vulkanVersion),  // Vulkan版本
    swapchainPreferences(traits.swapchainPreferences),  // 交换链偏好
    depthFormat(traits.depthFormat),  // 深度格式
    depthImageUsage(traits.depthImageUsage),  // 深度图像使用标志
    queueFlags(traits.queueFlags),  // 队列标志
    queuePriorities(traits.queuePriorities),  // 队列优先级
    imageAvailableSemaphoreWaitFlag(traits.imageAvailableSemaphoreWaitFlag),  // 图像可用信号量等待标志
    debugLayer(traits.debugLayer),  // 调试层
    synchronizationLayer(traits.synchronizationLayer),  // 同步层
    apiDumpLayer(traits.apiDumpLayer),  // API转储层
    debugUtils(traits.debugUtils),  // 调试工具
    device(traits.device),  // 设备
    instanceExtensionNames(traits.instanceExtensionNames),  // 实例扩展名称
    requestedLayers(traits.requestedLayers),  // 请求的层
    deviceExtensionNames(traits.deviceExtensionNames),  // 设备扩展名称
    deviceTypePreferences(traits.deviceTypePreferences),  // 设备类型偏好
    deviceFeatures(traits.deviceFeatures),  // 设备特性
    samples(traits.samples)  // 多重采样
{
}

// WindowTraits类的构造函数（仅标题）
// 使用指定的窗口标题创建窗口特性
// title: 窗口标题
WindowTraits::WindowTraits(const std::string& title) :
    windowTitle(title)  // 窗口标题
{
    defaults();  // 设置默认值
}

// WindowTraits类的构造函数（位置和尺寸）
// 使用指定的位置、尺寸和标题创建窗口特性
// in_x: 窗口X位置
// in_y: 窗口Y位置
// in_width: 窗口宽度
// in_height: 窗口高度
// title: 窗口标题
WindowTraits::WindowTraits(int32_t in_x, int32_t in_y, uint32_t in_width, uint32_t in_height, const std::string& title) :
    x(in_x),  // 窗口X位置
    y(in_y),  // 窗口Y位置
    width(in_width),  // 窗口宽度
    height(in_height),  // 窗口高度
    windowTitle(title)  // 窗口标题
{
    defaults();  // 设置默认值
}

// WindowTraits类的构造函数（尺寸和标题）
// 使用指定的尺寸和标题创建窗口特性
// in_width: 窗口宽度
// in_height: 窗口高度
// title: 窗口标题
WindowTraits::WindowTraits(uint32_t in_width, uint32_t in_height, const std::string& title) :
    width(in_width),  // 窗口宽度
    height(in_height),  // 窗口高度
    windowTitle(title)  // 窗口标题
{
    defaults();  // 设置默认值
}

// 设置默认值
// 初始化窗口特性的默认值
void WindowTraits::defaults()
{
#if !defined(__ANDROID__)
    // 查询可用的Vulkan实例版本
    vkEnumerateInstanceVersion(&vulkanVersion);
#endif

    // vsg::DeviceFeatures使用实例扩展
    instanceExtensionNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    // 默认启用各向异性过滤
    if (!deviceFeatures) deviceFeatures = vsg::DeviceFeatures::create();
    deviceFeatures->get().samplerAnisotropy = VK_TRUE;

    // 设备类型偏好：优先选择独立GPU，然后是集成GPU，最后是虚拟GPU
    deviceTypePreferences = {VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU};
}

// 验证窗口特性
// 根据启用的选项添加必要的扩展和层
void WindowTraits::validate()
{
    // 如果启用了调试层、API转储层或同步层，添加调试报告扩展
    if (debugLayer || apiDumpLayer || synchronizationLayer)
    {
        instanceExtensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
    // 如果启用了调试工具且扩展支持，添加调试工具扩展
    if (debugUtils && isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        instanceExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    // 根据启用的选项添加相应的层
    if (apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
    if (debugLayer) requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
    if (synchronizationLayer) requestedLayers.push_back("VK_LAYER_KHRONOS_synchronization2");

    // 验证并过滤请求的层名称
    requestedLayers = vsg::validateInstanceLayerNames(requestedLayers);
}
