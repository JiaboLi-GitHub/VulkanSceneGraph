/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/Window.h>
#include <vsg/commands/PipelineBarrier.h>
#include <vsg/core/Exception.h>
#include <vsg/core/Version.h>
#include <vsg/io/Logger.h>
#include <vsg/maths/color.h>
#include <vsg/maths/vec4.h>
#include <vsg/ui/ApplicationEvent.h>
#include <vsg/utils/CoordinateSpace.h>
#include <vsg/vk/SubmitCommands.h>

#include <array>
#include <chrono>

using namespace vsg;

#if VSG_SUPPORTS_Windowing == 0
ref_ptr<Window> Window::create(vsg::ref_ptr<WindowTraits>)
{
    return {};
}
#endif

// 构造函数：初始化窗口对象
// traits: 窗口特性配置，包含窗口大小、标题、Vulkan层和扩展等设置
Window::Window(ref_ptr<WindowTraits> traits) :
    _traits(traits),
    _extent2D{std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()},
    _clearColor{0.2f, 0.2f, 0.4f, 1.0f},
    _framebufferSamples(VK_SAMPLE_COUNT_1_BIT)
{
    // 如果交换链使用sRGB格式，需要将清除颜色从sRGB空间转换为线性空间
    // 这是因为Vulkan在渲染时使用线性颜色空间，而sRGB格式需要特殊处理
    if (_traits && (_traits->swapchainPreferences.surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB || _traits->swapchainPreferences.surfaceFormat.format == VK_FORMAT_B8G8R8_SRGB))
    {
        _clearColor = sRGB_to_linear(_clearColor);
    }
}

// 析构函数：清理窗口资源
Window::~Window()
{
}

// 清除所有窗口相关的Vulkan资源
// 释放交换链、深度缓冲区、渲染通道、表面、设备和物理设备等资源
void Window::clear()
{
    _frames.clear();
    _swapchain.reset();

    _depthImage.reset();
    _depthImageView.reset();

    _renderPass.reset();
    _surface.reset();
    _device.reset();
    _physicalDevice.reset();
}

// 共享设备并初始化窗口
// device: 要共享的Vulkan设备对象
// 设置设备后，初始化表面、格式和渲染通道
void Window::share(ref_ptr<Device> device)
{
    setDevice(device);
    _initSurface();
    _initFormats();
    _initRenderPass();
}

// 获取表面格式
// 返回: 交换链使用的表面格式（颜色格式和颜色空间）
// 如果设备未初始化，则先初始化设备
VkSurfaceFormatKHR Window::surfaceFormat()
{
    if (!_device) _initDevice();
    return _imageFormat;
}

// 获取深度缓冲区格式
// 返回: 深度缓冲区使用的Vulkan格式
// 如果设备未初始化，则先初始化设备
VkFormat Window::depthFormat()
{
    if (!_device) _initDevice();
    return _depthFormat;
}

void Window::setInstance(ref_ptr<Instance> instance)
{
    _instance = instance;
}

ref_ptr<Instance> Window::getOrCreateInstance()
{
    if (!_instance) _initInstance();
    return _instance;
}

void Window::setSurface(ref_ptr<Surface> surface)
{
    _surface = surface;
}

ref_ptr<Surface> Window::getOrCreateSurface()
{
    if (!_surface) _initSurface();
    return _surface;
}

void Window::setPhysicalDevice(ref_ptr<PhysicalDevice> physicalDevice)
{
    _physicalDevice = physicalDevice;
}

ref_ptr<PhysicalDevice> Window::getOrCreatePhysicalDevice()
{
    if (!_physicalDevice) _initPhysicalDevice();
    return _physicalDevice;
}

void Window::setDevice(ref_ptr<Device> device)
{
    _device = device;
    if (_device)
    {
        _physicalDevice = _device->getPhysicalDevice();
        _instance = _device->getInstance();
    }
}

ref_ptr<Device> Window::getOrCreateDevice()
{
    if (!_device) _initDevice();
    return _device;
}

void Window::setRenderPass(ref_ptr<RenderPass> renderPass)
{
    _renderPass = renderPass;
}

ref_ptr<RenderPass> Window::getOrCreateRenderPass()
{
    if (!_renderPass) _initRenderPass();
    return _renderPass;
}

ref_ptr<Swapchain> Window::getOrCreateSwapchain()
{
    if (!_swapchain) _initSwapchain();
    return _swapchain;
}

ref_ptr<Image> Window::getOrCreateDepthImage()
{
    if (!_depthImage) _initSwapchain();
    return _depthImage;
}

ref_ptr<ImageView> Window::getOrCreateDepthImageView()
{
    if (!_depthImageView) _initSwapchain();
    return _depthImageView;
}

// 初始化Vulkan实例
// 验证窗口特性，添加必需的实例扩展（表面扩展和平台特定的表面扩展），然后创建Vulkan实例
void Window::_initInstance()
{
    // 创建Vulkan实例
    _traits->validate();

    vsg::Names& instanceExtensions = _traits->instanceExtensionNames;

    // 添加必需的表面扩展
    instanceExtensions.push_back("VK_KHR_surface");
    // 添加平台特定的表面扩展（如VK_KHR_win32_surface、VK_KHR_xlib_surface等）
    instanceExtensions.push_back(instanceExtensionSurfaceName());

    // 创建Vulkan实例，包含扩展、验证层和Vulkan版本
    _instance = vsg::Instance::create(instanceExtensions, _traits->requestedLayers, _traits->vulkanVersion);
}

// 初始化物理设备
// 如果实例或表面未初始化，则先初始化它们
// 根据队列标志、表面和设备类型偏好选择适合的物理设备
void Window::_initPhysicalDevice()
{
    if (!_instance) _initInstance();
    if (!_surface) _initSurface();

    // 如果需要，设置物理设备
    if (!_physicalDevice)
    {
        // 根据队列标志、表面支持和设备类型偏好获取物理设备
        _physicalDevice = _instance->getPhysicalDevice(_traits->queueFlags, _surface, _traits->deviceTypePreferences);
        // 如果没有找到合适的物理设备，抛出异常
        if (!_physicalDevice) throw Exception{"Error: vsg::Window::create(...) failed to create Window,  no suitable Vulkan PhysicalDevice available.", VK_ERROR_INVALID_EXTERNAL_HANDLE};
    }
}

// 初始化格式
// 查询交换链支持详情，选择表面格式和深度格式，并计算可用的多重采样级别
void Window::_initFormats()
{
    // 查询物理设备和表面支持的交换链功能
    vsg::SwapChainSupportDetails supportDetails = vsg::querySwapChainSupport(*_physicalDevice, *_surface);

    // 从支持详情中选择合适的表面格式（优先使用偏好格式）
    _imageFormat = vsg::selectSwapSurfaceFormat(supportDetails, _traits->swapchainPreferences.surfaceFormat);
    // 使用特性中指定的深度格式
    _depthFormat = _traits->depthFormat;

    // 计算要使用的采样位数（多重采样抗锯齿）
    if (_traits->samples != VK_SAMPLE_COUNT_1_BIT)
    {
        // 获取设备支持的颜色和深度缓冲区多重采样级别
        VkSampleCountFlags deviceColorSamples = _physicalDevice->getProperties().limits.framebufferColorSampleCounts;
        VkSampleCountFlags deviceDepthSamples = _physicalDevice->getProperties().limits.framebufferDepthSampleCounts;
        // 找到同时满足颜色、深度和请求的采样级别
        VkSampleCountFlags satisfied = deviceColorSamples & deviceDepthSamples & _traits->samples;
        if (satisfied != 0)
        {
            // 选择满足条件的最高采样级别（使用位操作找到最高位）
            uint32_t highest = 1 << static_cast<uint32_t>(floor(log2(satisfied)));
            _framebufferSamples = static_cast<VkSampleCountFlagBits>(highest);
        }
        else
        {
            // 如果不支持多重采样，回退到单采样
            _framebufferSamples = VK_SAMPLE_COUNT_1_BIT;
        }
    }
    else
    {
        // 如果请求单采样，直接使用
        _framebufferSamples = VK_SAMPLE_COUNT_1_BIT;
    }
}

// 初始化逻辑设备
// 如果物理设备未初始化，则先初始化物理设备
// 设置设备扩展、队列族和队列设置，然后创建逻辑设备
void Window::_initDevice()
{
    // 如果需要，设置物理设备
    if (!_physicalDevice)
    {
        _initPhysicalDevice();
    }

    // 设置逻辑设备
    const vsg::Names& validatedNames = _traits->requestedLayers;

    // 准备设备扩展列表
    vsg::Names deviceExtensions;
    // 添加交换链扩展（必需）
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    // 添加用户请求的其他设备扩展
    deviceExtensions.insert(deviceExtensions.end(), _traits->deviceExtensionNames.begin(), _traits->deviceExtensionNames.end());

    // 获取图形队列族和呈现队列族
    auto [graphicsFamily, presentFamily] = _physicalDevice->getQueueFamily(_traits->queueFlags, _surface);
    // 如果找不到合适的队列族，抛出异常
    if (graphicsFamily < 0 || presentFamily < 0) throw Exception{"Error: vsg::Window::create(...) failed to create Window, no suitable Vulkan Device available.", VK_ERROR_INVALID_EXTERNAL_HANDLE};

    // 配置队列设置：图形队列和呈现队列
    vsg::QueueSettings queueSettings{vsg::QueueSetting{graphicsFamily, _traits->queuePriorities}, vsg::QueueSetting{presentFamily, {1.0}}};
    // 创建逻辑设备，包含物理设备、队列设置、验证层、设备扩展、设备特性和内存分配回调
    _device = vsg::Device::create(_physicalDevice, queueSettings, validatedNames, deviceExtensions, _traits->deviceFeatures, _instance->getAllocationCallbacks());

    // 初始化格式（需要在设备创建后调用）
    _initFormats();
}

// 初始化渲染通道
// 如果设备未初始化，则先初始化设备
// 根据是否使用多重采样创建相应的渲染通道
void Window::_initRenderPass()
{
    if (!_device) _initDevice();

    // 检查深度缓冲区是否需要作为传输源（用于读取深度值）
    bool requiresDepthRead = (_traits->depthImageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;

    // 根据是否使用多重采样选择创建单采样或多重采样渲染通道
    if (_framebufferSamples == VK_SAMPLE_COUNT_1_BIT)
    {
        // 创建单采样渲染通道
        _renderPass = vsg::createRenderPass(_device, _imageFormat.format, _depthFormat, requiresDepthRead);
    }
    else
    {
        // 创建多重采样渲染通道
        _renderPass = vsg::createMultisampledRenderPass(_device, _imageFormat.format, _depthFormat, _framebufferSamples, requiresDepthRead);
    }
}

// 初始化交换链
// 确保设备和渲染通道已初始化，然后构建交换链
void Window::_initSwapchain()
{
    if (!_device) _initDevice();
    if (!_renderPass) _initRenderPass();

    buildSwapchain();
}

// 构建交换链
// 如果交换链已存在，先等待设备空闲并清理旧资源
// 创建新的交换链、多重采样图像（如果需要）、深度缓冲区和帧缓冲区
// 为每个交换链图像创建信号量和帧缓冲区
void Window::buildSwapchain()
{
    if (_swapchain)
    {
        // 在删除关联资源之前，确保设备上的所有操作都已停止
        vkDeviceWaitIdle(*_device);

        // 在开始创建新交换链之前，清理之前的交换链
        _frames.clear();
        _indices.clear();

        _depthImageView.reset();
        _depthImage.reset();

        _multisampleImage.reset();
        _multisampleImageView.reset();
    }

    // 创建交换链（宽度和高度由表面控制，但这里也传入作为参考）
    _swapchain = Swapchain::create(_physicalDevice, _device, _surface, _extent2D.width, _extent2D.height, _traits->swapchainPreferences, _swapchain);

    // 获取交换链实际使用的尺寸
    _extent2D = _swapchain->getExtent();

    // 检查是否使用多重采样
    bool multisampling = _framebufferSamples != VK_SAMPLE_COUNT_1_BIT;
    if (multisampling)
    {
        // 创建多重采样颜色图像
        _multisampleImage = Image::create();
        _multisampleImage->imageType = VK_IMAGE_TYPE_2D;
        _multisampleImage->format = _imageFormat.format;
        _multisampleImage->extent.width = _extent2D.width;
        _multisampleImage->extent.height = _extent2D.height;
        _multisampleImage->extent.depth = 1;
        _multisampleImage->mipLevels = 1;
        _multisampleImage->arrayLayers = 1;
        _multisampleImage->samples = _framebufferSamples;
        _multisampleImage->tiling = VK_IMAGE_TILING_OPTIMAL;
        _multisampleImage->usage = _traits->swapchainPreferences.imageUsage;
        _multisampleImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        _multisampleImage->flags = 0;
        _multisampleImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        _multisampleImage->compile(_device);
        _multisampleImage->allocateAndBindMemory(_device);

        // 创建多重采样图像视图
        _multisampleImageView = ImageView::create(_multisampleImage, VK_IMAGE_ASPECT_COLOR_BIT);
        _multisampleImageView->compile(_device);
    }

    // 检查是否需要深度读取和深度解析
    bool requiresDepthRead = (_traits->depthImageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    bool requiresDepthResolve = (multisampling && requiresDepthRead);

    // 创建深度缓冲区
    _depthImage = Image::create();
    _depthImage->imageType = VK_IMAGE_TYPE_2D;
    _depthImage->extent.width = _extent2D.width;
    _depthImage->extent.height = _extent2D.height;
    _depthImage->extent.depth = 1;
    _depthImage->mipLevels = 1;
    _depthImage->arrayLayers = 1;
    _depthImage->format = _depthFormat;
    _depthImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    _depthImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    _depthImage->samples = _framebufferSamples;
    _depthImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    _depthImage->usage = _traits->depthImageUsage;

    _depthImage->compile(_device);
    _depthImage->allocateAndBindMemory(_device);

    // 创建深度图像视图
    _depthImageView = ImageView::create(_depthImage);
    _depthImageView->compile(_device);

    // 如果需要深度解析，创建额外的单采样深度缓冲区用于解析
    if (requiresDepthResolve)
    {
        // 保存多重采样深度图像
        _multisampleDepthImage = _depthImage;
        _multisampleDepthImageView = _depthImageView;

        // 创建单采样深度缓冲区用于解析
        _depthImage = Image::create();
        _depthImage->imageType = VK_IMAGE_TYPE_2D;
        _depthImage->extent.width = _extent2D.width;
        _depthImage->extent.height = _extent2D.height;
        _depthImage->extent.depth = 1;
        _depthImage->mipLevels = 1;
        _depthImage->arrayLayers = 1;
        _depthImage->format = _depthFormat;
        _depthImage->tiling = VK_IMAGE_TILING_OPTIMAL;
        _depthImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        _depthImage->usage = _traits->depthImageUsage;
        _depthImage->samples = VK_SAMPLE_COUNT_1_BIT;
        _depthImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        _depthImage->compile(_device);
        _depthImage->allocateAndBindMemory(_device);

        _depthImageView = ImageView::create(_depthImage);
        _depthImageView->compile(_device);
    }

    // 获取图形队列族
    int graphicsFamily = -1;
    std::tie(graphicsFamily, std::ignore) = _physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT, _surface);

    // 设置帧缓冲区和关联资源
    auto& imageViews = _swapchain->getImageViews();

    // 为每个交换链图像创建帧缓冲区和信号量
    size_t initial_indexValue = imageViews.size();
    for (size_t i = 0; i < imageViews.size(); ++i)
    {
        vsg::ImageViews attachments;
        // 如果使用多重采样，先添加多重采样颜色附件
        if (_multisampleImageView)
        {
            attachments.push_back(_multisampleImageView);
        }
        // 添加交换链图像视图（最终颜色附件）
        attachments.push_back(imageViews[i]);

        // 如果使用多重采样深度，先添加多重采样深度附件
        if (_multisampleDepthImageView)
        {
            attachments.push_back(_multisampleDepthImageView);
        }
        // 添加深度图像视图
        attachments.push_back(_depthImageView);

        // 创建帧缓冲区
        ref_ptr<Framebuffer> fb = Framebuffer::create(_renderPass, attachments, _extent2D.width, _extent2D.height, 1);

        // 创建图像可用信号量和渲染完成信号量
        ref_ptr<Semaphore> ias = vsg::Semaphore::create(_device, _traits->imageAvailableSemaphoreWaitFlag);
        ref_ptr<Semaphore> rfs = vsg::Semaphore::create(_device);

        // 存储帧信息：图像视图、帧缓冲区、图像可用信号量、渲染完成信号量
        _frames.push_back({imageViews[i], fb, ias, rfs});
        _indices.push_back(initial_indexValue);
    }

    // 创建额外的可用信号量池（用于获取下一帧图像）
    _availableSemaphoreIndex = 0;
    for (size_t i = 0; i < imageViews.size(); ++i)
    {
        _availableSemaphores.push_back(vsg::Semaphore::create(_device, _traits->imageAvailableSemaphoreWaitFlag));
    }

    {
        // 确保图像附件在GPU上正确设置
        auto commandPool = CommandPool::create(_device, graphicsFamily);
        submitCommandsToQueue(commandPool, _device->getQueue(graphicsFamily), [&](CommandBuffer& commandBuffer) {
            // 为深度图像创建内存屏障，将其布局从UNDEFINED转换为DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            auto depthImageBarrier = ImageMemoryBarrier::create(
                0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                _depthImage,
                _depthImageView->subresourceRange);

            auto pipelineBarrier = PipelineBarrier::create(
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0, depthImageBarrier);
            pipelineBarrier->record(commandBuffer);

            if (multisampling)
            {
                // 为多重采样颜色图像创建内存屏障
                auto msImageBarrier = ImageMemoryBarrier::create(
                    0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                    _multisampleImage,
                    VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
                auto msPipelineBarrier = PipelineBarrier::create(
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    0, msImageBarrier);
                msPipelineBarrier->record(commandBuffer);
            }
        });
    }
}

// 获取下一个交换链图像
// timeout: 等待超时时间（纳秒），0表示立即返回，UINT64_MAX表示无限等待
// 返回: VkResult，成功时返回VK_SUCCESS，交换链过期时返回VK_ERROR_OUT_OF_DATE_KHR
// 使用循环的信号量池来避免信号量冲突
VkResult Window::acquireNextImage(uint64_t timeout)
{
    if (!_swapchain) _initSwapchain();

    // 从信号量池中获取一个可用信号量（循环使用）
    auto& availableSemaphore = _availableSemaphores[_availableSemaphoreIndex];
    _availableSemaphoreIndex = (_availableSemaphoreIndex + 1) % _availableSemaphores.size();

    // 检查交换链和窗口尺寸是否一致，如果不一致返回过期错误
    if (_swapchain->getExtent() != _extent2D) return VK_ERROR_OUT_OF_DATE_KHR;

    // 从交换链获取下一个可用图像的索引
    uint32_t nextImageIndex;
    VkResult result = _swapchain->acquireNextImage(timeout, availableSemaphore, {}, nextImageIndex);

    if (result == VK_SUCCESS)
    {
        // 获取的图像信号量现在必须可用，将其与帧的图像可用信号量交换
        // 这样下一帧可以使用当前帧的信号量
        availableSemaphore.swap(_frames[nextImageIndex].imageAvailableSemaphore);

        // 向上移动之前的帧索引（历史记录）
        for (size_t i = _indices.size() - 1; i > 0; --i)
        {
            _indices[i] = _indices[i - 1];
        }

        // 更新索引头部为新获取的图像索引
        _indices[0] = nextImageIndex;
    }
    else
    {
        vsg::debug("Window::acquireNextImage(uint64_t timeout) _swapchain->acquireNextImage(...) failed with result = ", result);
    }

    return result;
}

// 轮询窗口事件
// events: 输出参数，用于接收事件列表
// 返回: 如果有事件则返回true，否则返回false
// 从缓冲事件队列中取出事件并添加到输出事件列表中
bool Window::pollEvents(vsg::UIEvents& events)
{
    if (!bufferedEvents.empty())
    {
        // 将缓冲事件移动到输出事件列表
        events.splice(events.end(), bufferedEvents);
        bufferedEvents.clear();
        return true;
    }

    return false;
}
