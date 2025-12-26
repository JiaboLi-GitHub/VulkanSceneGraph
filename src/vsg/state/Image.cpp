/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/state/Image.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 释放Vulkan数据
// 销毁Vulkan图像并释放设备内存
void Image::VulkanData::release()
{
    if (image)
    {
        vkDestroyImage(*device, image, device->getAllocationCallbacks());
        image = VK_NULL_HANDLE;
    }

    if (deviceMemory)
    {
        deviceMemory->release(memoryOffset, size);
        deviceMemory = {};
    }
}

// 构造函数：使用数据对象创建图像对象
// in_data: 数据对象（包含图像数据）
// 根据数据对象的属性自动设置图像类型、格式、尺寸、Mipmap级别等
// 支持1D、2D、3D、立方体贴图、数组等图像类型
// 自动将RGB格式转换为RGBA格式（Vulkan不支持RGB格式）
Image::Image(ref_ptr<Data> in_data) :
    data(in_data)
{
    if (data)
    {
        auto properties = data->properties;
        auto dimensions = data->dimensions();

        auto [width, height, depth] = data->pixelExtents();

        // 根据图像视图类型设置图像类型和数组层数
        switch (properties.imageViewType)
        {
        case (VK_IMAGE_VIEW_TYPE_1D):
            imageType = VK_IMAGE_TYPE_1D;
            arrayLayers = 1;
            break;
        case (VK_IMAGE_VIEW_TYPE_2D):
            imageType = VK_IMAGE_TYPE_2D;
            arrayLayers = 1;
            break;
        case (VK_IMAGE_VIEW_TYPE_3D):
            imageType = VK_IMAGE_TYPE_3D;
            arrayLayers = 1;
            break;
        case (VK_IMAGE_VIEW_TYPE_CUBE):
            imageType = VK_IMAGE_TYPE_2D;
            flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            arrayLayers = depth;
            depth = 1;
            break;
        case (VK_IMAGE_VIEW_TYPE_1D_ARRAY):
            imageType = VK_IMAGE_TYPE_1D;
            arrayLayers = height * depth;
            height = 1;
            depth = 1;
            /* flags = VK_IMAGE_CREATE_1D_ARRAY_COMPATIBLE_BIT; // 注释掉，因为Vulkan头文件尚未提供此标志 */
            break;
        case (VK_IMAGE_VIEW_TYPE_2D_ARRAY):
            // imageType = VK_IMAGE_TYPE_3D;
            // flags = VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
            imageType = VK_IMAGE_TYPE_2D;
            arrayLayers = depth;
            depth = 1;
            break;
        case (VK_IMAGE_VIEW_TYPE_CUBE_ARRAY):
            imageType = VK_IMAGE_TYPE_2D;
            flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            arrayLayers = depth;
            depth = 1;
            break;
        default:
            imageType = dimensions >= 3 ? VK_IMAGE_TYPE_3D : (dimensions == 2 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D);
            arrayLayers = 1;
            break;
        }

        format = properties.format;
        mipLevels = std::max(1u, static_cast<uint32_t>(data->properties.mipLevels));
        extent = VkExtent3D{width, height, depth};

        // vsg::info("Image::Image(", data, ") mpipLevels = ", mipLevels);

        // 将RGB格式重新映射为RGBA格式（Vulkan不支持RGB格式）
        if (format >= VK_FORMAT_R8G8B8_UNORM && format <= VK_FORMAT_B8G8R8_SRGB)
            format = static_cast<VkFormat>(format + 14);
        else if (format >= VK_FORMAT_R16G16B16_UNORM && format <= VK_FORMAT_R16G16B16_SFLOAT)
            format = static_cast<VkFormat>(format + 7);
        else if (format >= VK_FORMAT_R32G32B32_UINT && format <= VK_FORMAT_R32G32B32_SFLOAT)
            format = static_cast<VkFormat>(format + 3);

        // 设置默认使用标志：采样和传输目标
        usage = (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }
}

// 构造函数：使用现有Vulkan图像创建图像对象
// image: 现有的Vulkan图像句柄
// device: Vulkan设备对象
// 用于包装外部创建的Vulkan图像
Image::Image(VkImage image, Device* device)
{
    VulkanData& vd = _vulkanData[device->deviceID];
    vd.image = image;
    vd.device = device;
}

// 析构函数：销毁图像对象
// 释放所有设备的Vulkan数据
Image::~Image()
{
    for (auto& vd : _vulkanData) vd.release();
}

int Image::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_pointer(data, rhs.data))) return result;

    if ((result = compare_value(flags, rhs.flags))) return result;
    if ((result = compare_value(imageType, rhs.imageType))) return result;
    if ((result = compare_value(format, rhs.format))) return result;
    if ((result = compare_memory(extent, rhs.extent))) return result;
    if ((result = compare_value(mipLevels, rhs.mipLevels))) return result;
    if ((result = compare_value(arrayLayers, rhs.arrayLayers))) return result;
    if ((result = compare_value(samples, rhs.samples))) return result;
    if ((result = compare_value(tiling, rhs.tiling))) return result;
    if ((result = compare_value(usage, rhs.usage))) return result;
    if ((result = compare_value(sharingMode, rhs.sharingMode))) return result;
    if ((result = compare_value_container(queueFamilyIndices, rhs.queueFamilyIndices))) return result;
    return compare_value(initialLayout, rhs.initialLayout);
}

// 绑定设备内存到图像
// deviceMemory: 设备内存对象
// memoryOffset: 内存中的偏移量
// 返回: Vulkan结果代码
// 将设备内存绑定到图像，使图像可以使用该内存
VkResult Image::bind(DeviceMemory* deviceMemory, VkDeviceSize memoryOffset)
{
    VulkanData& vd = _vulkanData[deviceMemory->getDevice()->deviceID];

    VkResult result = vkBindImageMemory(*vd.device, vd.image, *deviceMemory, memoryOffset);
    if (result == VK_SUCCESS)
    {
        vd.deviceMemory = deviceMemory;
        vd.memoryOffset = memoryOffset;
    }
    return result;
}

// 分配并绑定设备内存
// device: Vulkan设备对象
// memoryProperties: 内存属性标志
// pNextAllocInfo: 可选的分配信息扩展
// 返回: Vulkan结果代码
// 获取内存需求，分配设备内存，并将内存绑定到图像
VkResult Image::allocateAndBindMemory(Device* device, VkMemoryPropertyFlags memoryProperties, void* pNextAllocInfo)
{
    auto memRequirements = getMemoryRequirements(device->deviceID);
    auto memory = DeviceMemory::create(device, memRequirements, memoryProperties, pNextAllocInfo);
    auto [allocated, offset] = memory->reserve(memRequirements.size);
    if (!allocated)
    {
        throw Exception{"Error: Failed to allocate DeviceMemory."};
    }
    return bind(memory, offset);
}

// 获取内存需求
// deviceID: 设备ID
// 返回: Vulkan内存需求结构
// 查询图像所需的内存大小、对齐和内存类型
VkMemoryRequirements Image::getMemoryRequirements(uint32_t deviceID) const
{
    const VulkanData& vd = _vulkanData[deviceID];

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*vd.device, vd.image, &memRequirements);
    return memRequirements;
}

// 编译图像（使用设备）
// device: Vulkan设备对象
// 创建Vulkan图像对象
void Image::compile(Device* device)
{
    auto& vd = _vulkanData[device->deviceID];
    if (vd.image != VK_NULL_HANDLE) return;

    // 设置图像创建信息
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    info.imageType = imageType;
    info.format = format;
    info.extent = extent;
    info.mipLevels = mipLevels;
    info.arrayLayers = arrayLayers;
    info.samples = samples;
    info.tiling = tiling;
    info.usage = usage;
    info.sharingMode = sharingMode;
    info.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
    info.pQueueFamilyIndices = queueFamilyIndices.data();
    info.initialLayout = initialLayout;

    // vsg::info("Image::compile(), data = ",data, ", mipLevels = ", mipLevels, ", arrayLayers = ", arrayLayers, ", extent = {", extent.width, ", ", extent.height, ", ", extent.depth, "}");

    vd.device = device;

    // 标记是否需要复制数据
    vd.requiresDataCopy = data.valid();

    // 创建Vulkan图像
    if (VkResult result = vkCreateImage(*vd.device, &info, vd.device->getAllocationCallbacks(), &vd.image); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create VkImage.", result};
    }
}

// 编译图像（使用上下文）
// context: 编译上下文对象
// 创建Vulkan图像对象，从设备内存缓冲区池分配内存并绑定
void Image::compile(Context& context)
{
    auto& vd = _vulkanData[context.deviceID];
    if (vd.image != VK_NULL_HANDLE) return;

    // 先创建图像
    compile(context.device);

    // 获取内存需求
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*vd.device, vd.image, &memRequirements);

    // 从设备内存缓冲区池预留内存
    auto [deviceMemory, offset] = context.deviceMemoryBufferPools->reserveMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (!deviceMemory)
    {
        throw Exception{"Error: Image failed to reserve slot from deviceMemoryBufferPools.", VK_ERROR_OUT_OF_DEVICE_MEMORY};
    }

    // 标记是否需要复制数据
    vd.requiresDataCopy = data.valid();

    // 绑定内存到图像
    bind(deviceMemory, offset);
}
