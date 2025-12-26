/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/io/Logger.h>
#include <vsg/vk/Device.h>
#include <vsg/vk/Surface.h>
#include <vsg/vk/Swapchain.h>

#include <algorithm>
#include <limits>

using namespace vsg;

// 查询交换链支持详情
// device: 物理设备句柄
// surface: 表面句柄
// 返回: 交换链支持详情（包括能力、格式、呈现模式）
// 查询物理设备对指定表面的交换链支持情况
SwapChainSupportDetails vsg::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;

    // 获取表面能力
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    // 获取表面格式
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    details.formats.resize(formatCount);
    if (formatCount > 0)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    // 获取呈现模式
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    details.presentModes.resize(presentModeCount);
    if (presentModeCount > 0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

// 选择交换链表面格式
// details: 交换链支持详情
// preferredSurfaceFormat: 首选表面格式
// 返回: 选定的表面格式
// 从可用的表面格式中选择一个，优先使用首选格式，否则使用回退格式
VkSurfaceFormatKHR vsg::selectSwapSurfaceFormat(const SwapChainSupportDetails& details, VkSurfaceFormatKHR preferredSurfaceFormat)
{
    if (details.formats.empty() || (details.formats.size() == 1 && details.formats[0].format == VK_FORMAT_UNDEFINED))
    {
        warn("selectSwapSurfaceFormat() VK_FORMAT_UNDEFINED, so using fallback ");
        return {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    // 检查请求的格式是否可用
    for (const auto& availableFormat : details.formats)
    {
        if (availableFormat.format == preferredSurfaceFormat.format && availableFormat.colorSpace == preferredSurfaceFormat.colorSpace)
        {
            return availableFormat;
        }
    }

    // 回退到检查{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
    for (const auto& availableFormat : details.formats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    // 回退到使用可用格式列表中的第一个
    return details.formats[0];
}

// 选择交换链范围
// details: 交换链支持详情
// width: 请求的宽度
// height: 请求的高度
// 返回: 选定的范围
// 根据表面能力和请求的尺寸选择交换链图像范围
VkExtent2D vsg::selectSwapExtent(const SwapChainSupportDetails& details, uint32_t width, uint32_t height)
{
    const VkSurfaceCapabilitiesKHR& capabilities = details.capabilities;

    // 如果当前范围已定义，使用它
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        // 否则在最小和最大范围之间选择
        VkExtent2D extent;
        extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, width));
        extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, height));
        return extent;
    }
}

// 选择交换链呈现模式
// details: 交换链支持详情
// preferredPresentMode: 首选呈现模式
// 返回: 选定的呈现模式
// 从可用的呈现模式中选择一个，优先使用首选模式，否则使用回退模式（MAILBOX或FIFO）
VkPresentModeKHR vsg::selectSwapPresentMode(const SwapChainSupportDetails& details, VkPresentModeKHR preferredPresentMode)
{
    // 如果请求的呈现模式可用，选择它
    for (auto availablePresentMode : details.presentModes)
    {
        if (availablePresentMode == preferredPresentMode) return availablePresentMode;
    }

    // 请求的呈现模式不可用，回退到检查VK_PRESENT_MODE_MAILBOX_KHR是否可用
    for (auto availablePresentMode : details.presentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode;
    }

    // 回退到VK_PRESENT_MODE_FIFO_KHR（始终可用）
    return VK_PRESENT_MODE_FIFO_KHR;

    /**
    From https://github.com/LunarG/VulkanSamples/issues/98 :

    VK_PRESENT_MODE_IMMEDIATE_KHR. This is for applications that don't care about tearing, or have some way of synchronizing with the display (which Vulkan doesn't yet provide).

    VK_PRESENT_MODE_FIFO_KHR. This is for applications that don't want tearing ever. It's difficult to say how fast they may be, whether they care about stuttering/latency.

    VK_PRESENT_MODE_FIFO_RELAXED_KHR. This is for applications that generally render/present a new frame every refresh cycle, but are occasionally late. In this case (perhaps because of stuttering/latency concerns), they want the late image to be immediately displayed, even though that may mean some tearing.

    VK_PRESENT_MODE_MAILBOX_KHR. I'm guessing that this is for applications that generally render/present a new frame every refresh cycle, but are occasionally early. In this case, they want the new image to be displayed instead of the previously-queued-for-presentation image that has not yet been displayed.
    **/
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// SwapchainImage - 交换链图像辅助类，禁用自动清理（交换链管理其生命周期）
//
namespace vsg
{
    // 辅助类，禁用交换链图像的自动清理，因为交换链本身管理其生命周期
    class SwapchainImage : public Inherit<Image, SwapchainImage>
    {
    public:
        SwapchainImage(VkImage image, Device* device) :
            Inherit(image, device)
        {
        }

    protected:
        virtual ~SwapchainImage()
        {
            // 清除Vulkan数据，但不销毁图像（由交换链管理）
            for (auto& vd : _vulkanData)
            {
                vd.deviceMemory = nullptr;
                vd.image = VK_NULL_HANDLE;
            }
        }
    };
    VSG_type_name(vsg::SwapchainImage);

} // namespace vsg

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Swapchain - 交换链，管理用于呈现的图像
//
// 构造函数：创建交换链对象
// physicalDevice: 物理设备对象
// device: 逻辑设备对象
// surface: 表面对象
// width: 交换链图像宽度
// height: 交换链图像高度
// preferences: 交换链首选项（将被更新为实际使用的值）
// oldSwapchain: 旧的交换链（可选，用于重建交换链）
// 交换链管理用于呈现到窗口的图像队列
Swapchain::Swapchain(PhysicalDevice* physicalDevice, Device* device, Surface* surface, uint32_t width, uint32_t height, SwapchainPreferences& preferences, ref_ptr<Swapchain> oldSwapchain) :
    _device(device)
{
    SwapChainSupportDetails details = querySwapChainSupport(*physicalDevice, *surface);

    // 选择表面格式、呈现模式和范围
    VkSurfaceFormatKHR surfaceFormat = selectSwapSurfaceFormat(details, preferences.surfaceFormat);
    VkPresentModeKHR presentMode = selectSwapPresentMode(details, preferences.presentMode);
    VkExtent2D extent = selectSwapExtent(details, width, height);

    // 确定图像数量（在最小和最大之间）
    uint32_t imageCount = std::max(preferences.imageCount, details.capabilities.minImageCount);                        // Vulkan规范要求minImageCount至少为1
    if (details.capabilities.maxImageCount > 0) imageCount = std::min(imageCount, details.capabilities.maxImageCount); // Vulkan规范指定0表示无限制的图像数量

    // 将选定的设置应用回首选项，以便调用代码可以确定实际使用的设置
    preferences.imageCount = imageCount;
    preferences.presentMode = presentMode;
    preferences.surfaceFormat = surfaceFormat;

    debug("Swapchain::create(...., width = ", width, ", height = ", height, ")");
    debug("     details.capabilities.minImageCount=", details.capabilities.minImageCount);
    debug("     details.capabilities.maxImageCount=", details.capabilities.maxImageCount);
    debug("     imageCount = ", imageCount);

    // 创建交换链
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = *surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = preferences.imageUsage;

    // 确定图像共享模式（如果图形队列和呈现队列不同，使用并发模式）
    auto [graphicsFamily, presentFamily] = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT, surface);
    uint32_t queueFamilyIndices[] = {uint32_t(graphicsFamily), uint32_t(presentFamily)};
    if (graphicsFamily != presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = details.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    // 如果提供了旧交换链，使用它（用于重建交换链）
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    if (oldSwapchain)
    {
        createInfo.oldSwapchain = *(oldSwapchain);
    }

    createInfo.pNext = nullptr;

    VkSwapchainKHR swapchain;
    VkResult result = vkCreateSwapchainKHR(*device, &createInfo, _device->getAllocationCallbacks(), &swapchain);
    if (result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create swap chain.", result};
    }

    // 将数据分配给此Swapchain对象
    _surface = surface;
    _swapchain = swapchain;

    _format = surfaceFormat.format;
    _extent = extent;

    // 创建图像视图
    vkGetSwapchainImagesKHR(*device, swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(*device, swapchain, &imageCount, images.data());

    // 为每个交换链图像创建图像视图
    for (std::size_t i = 0; i < images.size(); ++i)
    {
        auto imageView = ImageView::create(SwapchainImage::create(images[i], device));
        imageView->viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageView->format = surfaceFormat.format;
        imageView->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageView->subresourceRange.baseMipLevel = 0;
        imageView->subresourceRange.levelCount = 1;
        imageView->subresourceRange.baseArrayLayer = 0;
        imageView->subresourceRange.layerCount = 1;
        imageView->compile(device);

        _imageViews.push_back(imageView);
    }
}

// 析构函数：销毁交换链对象
// 清理图像视图并销毁Vulkan交换链
Swapchain::~Swapchain()
{
    _imageViews.clear();

    if (_swapchain)
    {
        debug("Calling vkDestroySwapchainKHR(..)");
        vkDestroySwapchainKHR(*_device, _swapchain, _device->getAllocationCallbacks());
    }
}

// 获取下一个可用的交换链图像
// timeout: 超时时间（纳秒）
// semaphore: 信号量（可选，用于同步）
// fence: 围栏（可选，用于同步）
// imageIndex: 输出参数，图像索引
// 返回: Vulkan结果
// 获取下一个可用于呈现的交换链图像索引
VkResult Swapchain::acquireNextImage(uint64_t timeout, ref_ptr<Semaphore> semaphore, ref_ptr<Fence> fence, uint32_t& imageIndex)
{
    VkSemaphore vk_semaphore = semaphore ? semaphore->vk() : VK_NULL_HANDLE;
    VkFence vk_fence = fence ? fence->vk() : VK_NULL_HANDLE;
    return vkAcquireNextImageKHR(*_device, _swapchain, timeout, vk_semaphore, vk_fence, &imageIndex);
}
