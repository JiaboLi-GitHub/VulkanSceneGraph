/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/View.h>
#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/state/DescriptorSetLayout.h>
#include <vsg/vk/Context.h>

using namespace vsg;

//////////////////////////////////////
//
// DescriptorSetLayout
//
// 构造函数：创建描述符集布局对象（默认）
// 描述符集布局用于定义描述符集的结构（绑定点、类型、数量等）
DescriptorSetLayout::DescriptorSetLayout()
{
}

// 拷贝构造函数：从另一个描述符集布局对象创建新的描述符集布局对象
// rhs: 要拷贝的描述符集布局对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝创建标志、绑定列表和绑定标志列表
DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetLayout& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    createFlags(rhs.createFlags),
    bindings(rhs.bindings),
    bindingFlags(rhs.bindingFlags)
{
}

// 构造函数：使用描述符集布局绑定列表创建描述符集布局对象
// descriptorSetLayoutBindings: 描述符集布局绑定列表
DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetLayoutBindings& descriptorSetLayoutBindings) :
    bindings(descriptorSetLayoutBindings)
{
}

// 析构函数：销毁描述符集布局对象
DescriptorSetLayout::~DescriptorSetLayout()
{
}

// 添加绑定到描述符集布局
// binding: 绑定点索引
// descriptorType: 描述符类型（统一缓冲区、采样器等）
// descriptorCount: 描述符数量（用于数组）
// stageFlags: 着色器阶段标志（指定哪些着色器阶段可以访问）
// flags: 绑定标志（如动态描述符、部分更新等）
// 添加一个描述符绑定到布局，如果提供了绑定标志则记录
void DescriptorSetLayout::addBinding(uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount, VkShaderStageFlags stageFlags, VkDescriptorBindingFlags flags)
{
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = descriptorType;
    layoutBinding.descriptorCount = descriptorCount;
    layoutBinding.stageFlags = stageFlags;
    layoutBinding.pImmutableSamplers = nullptr;
    bindings.push_back(layoutBinding);

    // 如果提供了绑定标志，记录它
    if (flags != 0)
    {
        if (binding >= bindingFlags.size()) bindingFlags.resize(binding + 1, 0);
        bindingFlags[binding] = flags;
    }
}

// 获取描述符池大小
// descriptorPoolSizes: 输出参数，用于填充描述符池大小列表
// 根据绑定列表计算所需的描述符池大小（按类型分组并累加数量）
void DescriptorSetLayout::getDescriptorPoolSizes(DescriptorPoolSizes& descriptorPoolSizes)
{
    for (auto& binding : bindings)
    {
        // 查找是否已有相同类型的描述符池大小条目
        auto itr = descriptorPoolSizes.begin();
        for (; itr != descriptorPoolSizes.end(); ++itr)
        {
            if (itr->type == binding.descriptorType)
            {
                // 累加描述符数量
                itr->descriptorCount += binding.descriptorCount;
                break;
            }
        }
        // 如果没有找到，添加新条目
        if (itr == descriptorPoolSizes.end())
        {
            descriptorPoolSizes.emplace_back(VkDescriptorPoolSize{binding.descriptorType, binding.descriptorCount});
        }
    }
}

int DescriptorSetLayout::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(createFlags, rhs.createFlags))) return result;
    if ((result = compare_value_container(bindings, rhs.bindings))) return result;

    return compare_value_container(bindingFlags, rhs.bindingFlags);
}

void DescriptorSetLayout::read(Input& input)
{
    Object::read(input);

    if (input.version_greater_equal(1, 1, 12))
    {
        createFlags = input.readValue<uint32_t>("createFlags");
    }

    bindings.resize(input.readValue<uint32_t>("bindings"));
    for (auto& dslb : bindings)
    {
        input.read("binding", dslb.binding);
        input.readValue<uint32_t>("descriptorType", dslb.descriptorType);
        input.read("descriptorCount", dslb.descriptorCount);
        input.readValue<uint32_t>("stageFlags", dslb.stageFlags);
    }

    if (input.version_greater_equal(1, 1, 12))
    {
        input.readValues("bindingFlags", bindingFlags);
    }
}

void DescriptorSetLayout::write(Output& output) const
{
    Object::write(output);

    if (output.version_greater_equal(1, 1, 12))
    {
        output.writeValue<uint32_t>("createFlags", createFlags);
    }

    output.writeValue<uint32_t>("bindings", bindings.size());
    for (auto& dslb : bindings)
    {
        output.write("binding", dslb.binding);
        output.writeValue<uint32_t>("descriptorType", dslb.descriptorType);
        output.write("descriptorCount", dslb.descriptorCount);
        output.writeValue<uint32_t>("stageFlags", dslb.stageFlags);
    }

    if (output.version_greater_equal(1, 1, 12))
    {
        output.writeValues("bindingFlags", bindingFlags);
    }
}

// 编译描述符集布局
// context: 编译上下文对象
// 创建Vulkan描述符集布局对象
void DescriptorSetLayout::compile(Context& context)
{
    if (!_implementation[context.deviceID]) _implementation[context.deviceID] = DescriptorSetLayout::Implementation::create(context.device, createFlags, bindings, bindingFlags);
}

//////////////////////////////////////
//
// DescriptorSetLayout::Implementation
//
// 实现类构造函数：创建描述符集布局实现对象
// device: Vulkan设备对象
// createFlags: 创建标志
// descriptorSetLayoutBindings: 描述符集布局绑定列表
// descriptorSetLayoutBindingFlags: 描述符集布局绑定标志列表
// 设置描述符集布局创建信息，如果提供了绑定标志则设置扩展结构，然后创建Vulkan描述符集布局对象
DescriptorSetLayout::Implementation::Implementation(Device* device, VkDescriptorSetLayoutCreateFlags createFlags, const DescriptorSetLayoutBindings& descriptorSetLayoutBindings, const DescriptorSetLayoutBindingFlags& descriptorSetLayoutBindingFlags) :
    _device(device)
{
    // 设置描述符集布局创建信息
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
    layoutInfo.pBindings = descriptorSetLayoutBindings.data();
    layoutInfo.flags = createFlags;

    // 如果提供了绑定标志，设置扩展结构
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo = {};
    if (descriptorSetLayoutBindingFlags.empty())
    {
        layoutInfo.pNext = nullptr;
    }
    else
    {
        bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindingFlags.size());
        bindingFlagsCreateInfo.pBindingFlags = descriptorSetLayoutBindingFlags.data();
        layoutInfo.pNext = &bindingFlagsCreateInfo;
    }

    // 创建Vulkan描述符集布局
    if (VkResult result = vkCreateDescriptorSetLayout(*device, &layoutInfo, _device->getAllocationCallbacks(), &_descriptorSetLayout); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create DescriptorSetLayout.", result};
    }
}

// 实现类析构函数：销毁描述符集布局实现对象
// 释放Vulkan描述符集布局对象
DescriptorSetLayout::Implementation::~Implementation()
{
    if (_descriptorSetLayout)
    {
        vkDestroyDescriptorSetLayout(*_device, _descriptorSetLayout, _device->getAllocationCallbacks());
    }
}
