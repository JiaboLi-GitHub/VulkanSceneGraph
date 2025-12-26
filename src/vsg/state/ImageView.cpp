/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/state/ImageView.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 根据格式计算方面标志
// format: Vulkan格式
// 返回: 图像方面标志（颜色、深度、模板）
// 根据图像格式确定图像方面（深度格式返回深度位，深度模板格式返回深度和模板位，其他返回颜色位）
VkImageAspectFlags vsg::computeAspectFlagsForFormat(VkFormat format)
{
    if (format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else if (format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_X8_D24_UNORM_PACK32)
    {
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    else
    {
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

// 释放Vulkan数据
// 销毁Vulkan图像视图
void ImageView::VulkanData::release()
{
    if (imageView)
    {
        vkDestroyImageView(*device, imageView, device->getAllocationCallbacks());
        imageView = VK_NULL_HANDLE;
        device = {};
    }
}

// 构造函数：使用图像创建图像视图对象（自动计算方面标志）
// in_image: 图像对象
// 根据图像类型和数据属性自动设置视图类型、格式和子资源范围
ImageView::ImageView(ref_ptr<Image> in_image) :
    image(in_image)
{
    if (image)
    {
        // 根据数据属性或图像类型确定视图类型
        if (image->data && image->data->properties.imageViewType >= 0)
        {
            viewType = static_cast<VkImageViewType>(image->data->properties.imageViewType);
        }
        else
        {
            auto imageType = image->imageType;
            viewType = (imageType == VK_IMAGE_TYPE_3D) ? VK_IMAGE_VIEW_TYPE_3D : ((imageType == VK_IMAGE_TYPE_2D) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_1D);
        }

        format = image->format;
        // 根据格式自动计算方面标志
        subresourceRange.aspectMask = computeAspectFlagsForFormat(image->format);
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = image->arrayLayers;
    }
}

// 构造函数：使用图像和方面标志创建图像视图对象
// in_image: 图像对象
// aspectFlags: 图像方面标志（指定访问图像的哪些方面：颜色、深度、模板）
// 根据图像类型和数据属性自动设置视图类型和格式，使用指定的方面标志
ImageView::ImageView(ref_ptr<Image> in_image, VkImageAspectFlags aspectFlags) :
    image(in_image)
{
    if (image)
    {
        // 根据数据属性或图像类型确定视图类型
        if (image->data && image->data->properties.imageViewType >= 0)
        {
            viewType = static_cast<VkImageViewType>(image->data->properties.imageViewType);
        }
        else
        {
            auto imageType = image->imageType;
            viewType = (imageType == VK_IMAGE_TYPE_3D) ? VK_IMAGE_VIEW_TYPE_3D : ((imageType == VK_IMAGE_TYPE_2D) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_1D);
        }

        format = image->format;
        // 使用指定的方面标志
        subresourceRange.aspectMask = aspectFlags;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = image->arrayLayers;
    }
}

// 析构函数：销毁图像视图对象
// 释放所有设备的Vulkan数据
ImageView::~ImageView()
{
    for (auto& vd : _vulkanData) vd.release();
}

int ImageView::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(flags, rhs.flags))) return result;
    if ((result = compare_pointer(image, rhs.image))) return result;
    if ((result = compare_value(viewType, rhs.viewType))) return result;
    if ((result = compare_memory(components, rhs.components))) return result;
    return compare_memory(subresourceRange, rhs.subresourceRange);
}

// 编译图像视图（使用设备）
// device: Vulkan设备对象
// 创建Vulkan图像视图对象
void ImageView::compile(Device* device)
{
    auto& vd = _vulkanData[device->deviceID];
    if (vd.imageView != VK_NULL_HANDLE) return;

    vd.device = device;

    // 设置图像视图创建信息
    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    info.viewType = viewType;
    info.format = format;
    info.components = components;
    info.subresourceRange = subresourceRange;

    if (image)
    {
        // 确保图像已编译
        image->compile(device);

        info.image = image->vk(device->deviceID);
    }

    // 创建Vulkan图像视图
    if (VkResult result = vkCreateImageView(*vd.device, &info, vd.device->getAllocationCallbacks(), &vd.imageView); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create VkImageView.", result};
    }
}

// 编译图像视图（使用上下文）
// context: 编译上下文对象
// 创建Vulkan图像视图对象，确保图像已编译
void ImageView::compile(Context& context)
{
    auto& vd = _vulkanData[context.deviceID];
    if (vd.imageView != VK_NULL_HANDLE) return;

    vd.device = context.device;

    // 设置图像视图创建信息
    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    info.viewType = viewType;
    info.format = format;
    info.components = components;
    info.subresourceRange = subresourceRange;

    if (image)
    {
        // 确保图像已编译
        image->compile(context);

        info.image = image->vk(vd.device->deviceID);
    }

    // 创建Vulkan图像视图
    if (VkResult result = vkCreateImageView(*vd.device, &info, vd.device->getAllocationCallbacks(), &vd.imageView); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create VkImageView.", result};
    }
}

// 创建图像视图（使用上下文）
// context: 编译上下文对象
// image: 图像对象
// aspectFlags: 图像方面标志
// 返回: 已编译的图像视图对象
// 编译图像，从设备内存缓冲区池分配内存并绑定，然后创建并编译图像视图
ref_ptr<ImageView> vsg::createImageView(vsg::Context& context, ref_ptr<Image> image, VkImageAspectFlags aspectFlags)
{
    vsg::Device* device = context.device;

    image->compile(device);

    // 获取内存需求
    VkMemoryRequirements memRequirements = image->getMemoryRequirements(device->deviceID);

    // 从设备内存缓冲区池预留内存（不使用导出内存信息扩展）
    auto [deviceMemory, offset] = context.deviceMemoryBufferPools->reserveMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    image->bind(deviceMemory, offset);

    auto imageView = ImageView::create(image, aspectFlags);
    imageView->compile(device);

    return imageView;
}

// 创建图像视图（使用设备）
// device: Vulkan设备对象
// image: 图像对象
// aspectFlags: 图像方面标志
// 返回: 已编译的图像视图对象
// 编译图像，分配并绑定内存，然后创建并编译图像视图
ref_ptr<ImageView> vsg::createImageView(Device* device, ref_ptr<Image> image, VkImageAspectFlags aspectFlags)
{
    image->compile(device);

    image->allocateAndBindMemory(device);

    auto imageView = ImageView::create(image, aspectFlags);
    imageView->compile(device);

    return imageView;
}
