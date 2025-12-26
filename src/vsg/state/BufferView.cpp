/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/state/BufferView.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 释放Vulkan数据
// 销毁Vulkan缓冲区视图
void BufferView::VulkanData::release()
{
    if (bufferView)
    {
        vkDestroyBufferView(*device, bufferView, device->getAllocationCallbacks());
        bufferView = VK_NULL_HANDLE;
        device = {};
    }
}

// 构造函数：创建缓冲区视图对象（默认）
// 缓冲区视图用于将缓冲区的一部分作为格式化数据访问（用于统一texel缓冲区、存储texel缓冲区等）
BufferView::BufferView()
{
}

// 构造函数：使用缓冲区、格式、偏移量和范围创建缓冲区视图对象
// in_buffer: 缓冲区对象
// in_format: 数据格式（用于解释缓冲区数据）
// in_offset: 缓冲区中的偏移量（字节）
// in_range: 范围大小（字节）
BufferView::BufferView(ref_ptr<Buffer> in_buffer, VkFormat in_format, VkDeviceSize in_offset, VkDeviceSize in_range) :
    buffer(in_buffer),
    format(in_format),
    offset(in_offset),
    range(in_range)
{
}

// 析构函数：销毁缓冲区视图对象
// 释放所有设备的Vulkan数据
BufferView::~BufferView()
{
    for (auto& vd : _vulkanData) vd.release();
}

// 比较两个缓冲区视图对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、缓冲区、格式、偏移量和范围
int BufferView::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_pointer(buffer, rhs.buffer))) return result;
    if ((result = compare_value(format, rhs.format))) return result;
    if ((result = compare_value(offset, rhs.offset))) return result;
    return compare_value(range, rhs.range);
}

// 编译缓冲区视图（使用设备）
// device: Vulkan设备对象
// 创建Vulkan缓冲区视图对象
void BufferView::compile(Device* device)
{
    auto& vd = _vulkanData[device->deviceID];
    if (vd.bufferView != VK_NULL_HANDLE) return;

    // 设置缓冲区视图创建信息
    VkBufferViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    createInfo.buffer = buffer->vk(device->deviceID);
    createInfo.format = format;
    createInfo.offset = offset;
    createInfo.range = range;
    createInfo.pNext = nullptr;

    vd.device = device;

    // 创建Vulkan缓冲区视图
    if (VkResult result = vkCreateBufferView(*device, &createInfo, device->getAllocationCallbacks(), &vd.bufferView); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create BufferView.", result};
    }
}

// 编译缓冲区视图（使用上下文）
// context: 编译上下文对象
// 先编译缓冲区，然后创建Vulkan缓冲区视图对象
void BufferView::compile(Context& context)
{
    buffer->compile(context);

    compile(context.device);
}
