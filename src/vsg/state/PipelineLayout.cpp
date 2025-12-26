/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/state/PipelineLayout.h>
#include <vsg/vk/Context.h>

using namespace vsg;

//////////////////////////////////////
//
// PipelineLayout
//
// 构造函数：创建管线布局对象（默认）
// 管线布局用于定义描述符集布局和推送常量范围
PipelineLayout::PipelineLayout() :
    flags(0)
{
}

// 拷贝构造函数：从另一个管线布局对象创建新的管线布局对象
// rhs: 要拷贝的管线布局对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝标志、描述符集布局列表和推送常量范围列表
PipelineLayout::PipelineLayout(const PipelineLayout& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    flags(rhs.flags),
    setLayouts(rhs.setLayouts),
    pushConstantRanges(rhs.pushConstantRanges)
{
}

// 构造函数：使用描述符集布局列表、推送常量范围列表和标志创建管线布局对象
// in_setLayouts: 描述符集布局列表
// in_pushConstantRanges: 推送常量范围列表
// in_flags: 创建标志
PipelineLayout::PipelineLayout(const DescriptorSetLayouts& in_setLayouts, const PushConstantRanges& in_pushConstantRanges, VkPipelineLayoutCreateFlags in_flags) :
    flags(in_flags),
    setLayouts(in_setLayouts),
    pushConstantRanges(in_pushConstantRanges)
{
}

// 析构函数：销毁管线布局对象
PipelineLayout::~PipelineLayout()
{
}

// 计算与另一个管线布局的兼容性
// other: 要比较的管线布局对象
// 返回: 兼容性结果对（第一个元素表示是否兼容，第二个元素表示兼容的描述符集数量）
// 检查推送常量范围和描述符集布局的兼容性（用于图形管线库）
std::pair<bool, uint32_t> vsg::PipelineLayout::computeCompatibility(const PipelineLayout& other)
{
    auto result = std::make_pair<bool, uint32_t>(compare_value_container(pushConstantRanges, other.pushConstantRanges) == 0, 0);
    if (!result.first)
        return result;
#ifdef VK_EXT_graphics_pipeline_library
    // 检查独立集标志是否匹配
    if ((flags & VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT) != (other.flags & VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT))
        return result;
#endif
    // 检查描述符集布局的兼容性
    for (result.second = 0; result.second < std::min(setLayouts.size(), other.setLayouts.size()); ++result.second)
    {
        // 如果这是图形管线库的部分布局，可能稍后会使其兼容
        if (!setLayouts[result.second] || !other.setLayouts[result.second])
            continue;
        if (compare_value_container(setLayouts[result.second]->bindings, other.setLayouts[result.second]->bindings) != 0)
            break;
    }
    return result;
}

int PipelineLayout::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(flags, rhs.flags))) return result;
    if ((result = compare_pointer_container(setLayouts, rhs.setLayouts))) return result;
    return compare_value_container(pushConstantRanges, rhs.pushConstantRanges);
}

void PipelineLayout::read(Input& input)
{
    Object::read(input);

    input.readValue<uint32_t>("flags", flags);

    setLayouts.resize(input.readValue<uint32_t>("setLayouts"));
    for (auto& descriptorLayout : setLayouts)
    {
        input.readObject("descriptorLayout", descriptorLayout);
    }

    pushConstantRanges.resize(input.readValue<uint32_t>("pushConstantRanges"));
    for (auto& pushConstantRange : pushConstantRanges)
    {
        input.readValue<uint32_t>("stageFlags", pushConstantRange.stageFlags);
        input.read("offset", pushConstantRange.offset);
        input.read("size", pushConstantRange.size);
    }
}

void PipelineLayout::write(Output& output) const
{
    Object::write(output);

    output.writeValue<uint32_t>("flags", flags);

    output.writeValue<uint32_t>("setLayouts", setLayouts.size());
    for (const auto& descriptorLayout : setLayouts)
    {
        output.writeObject("descriptorLayout", descriptorLayout);
    }

    output.writeValue<uint32_t>("pushConstantRanges", pushConstantRanges.size());
    for (const auto& pushConstantRange : pushConstantRanges)
    {
        output.writeValue<uint32_t>("stageFlags", pushConstantRange.stageFlags);
        output.write("offset", pushConstantRange.offset);
        output.write("size", pushConstantRange.size);
    }
}

// 编译管线布局
// context: 编译上下文对象
// 编译所有描述符集布局，然后创建Vulkan管线布局对象
void PipelineLayout::compile(Context& context)
{
    if (!_implementation[context.deviceID])
    {
        // 编译所有描述符集布局
        for (auto dsl : setLayouts)
        {
            if (dsl) dsl->compile(context);
        }
        // 创建管线布局实现
        _implementation[context.deviceID] = PipelineLayout::Implementation::create(context.device, setLayouts, pushConstantRanges, flags);
    }
}

//////////////////////////////////////
//
// PipelineLayout::Implementation
//
// 实现类构造函数：创建管线布局实现对象
// device: Vulkan设备对象
// descriptorSetLayouts: 描述符集布局列表
// pushConstantRanges: 推送常量范围列表
// flags: 创建标志
// 收集所有描述符集布局的Vulkan句柄，然后创建Vulkan管线布局对象
PipelineLayout::Implementation::Implementation(Device* device, const DescriptorSetLayouts& descriptorSetLayouts, const PushConstantRanges& pushConstantRanges, VkPipelineLayoutCreateFlags flags) :
    _device(device)
{
    // 收集所有描述符集布局的Vulkan句柄
    std::vector<VkDescriptorSetLayout> layouts;
    for (auto& dsl : descriptorSetLayouts)
    {
        if (dsl)
            layouts.push_back(dsl->vk(device->deviceID));
        else
            layouts.push_back(0);
    }

    // 设置管线布局创建信息
    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.flags = flags;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    pipelineLayoutInfo.pSetLayouts = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
    pipelineLayoutInfo.pNext = nullptr;

    // 创建Vulkan管线布局
    if (VkResult result = vkCreatePipelineLayout(*device, &pipelineLayoutInfo, _device->getAllocationCallbacks(), &_pipelineLayout); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create PipelineLayout.", result};
    }
}

// 实现类析构函数：销毁管线布局实现对象
// 释放Vulkan管线布局对象
PipelineLayout::Implementation::~Implementation()
{
    if (_pipelineLayout)
    {
        vkDestroyPipelineLayout(*_device, _pipelineLayout, _device->getAllocationCallbacks());
    }
}
