/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/View.h>
#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/state/DescriptorSet.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建描述符集对象（默认）
// 描述符集用于组织和管理一组描述符，绑定到着色器
DescriptorSet::DescriptorSet()
{
}

// 拷贝构造函数：从另一个描述符集对象创建新的描述符集对象
// rhs: 要拷贝的描述符集对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝描述符集布局和描述符列表
DescriptorSet::DescriptorSet(const DescriptorSet& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    setLayout(copyop(rhs.setLayout)),
    descriptors(copyop(rhs.descriptors))
{
}

// 构造函数：使用描述符集布局和描述符列表创建描述符集对象
// in_descriptorSetLayout: 描述符集布局（定义描述符的结构）
// in_descriptors: 描述符列表（包含要绑定的资源）
DescriptorSet::DescriptorSet(ref_ptr<DescriptorSetLayout> in_descriptorSetLayout, const Descriptors& in_descriptors) :
    setLayout(in_descriptorSetLayout),
    descriptors(in_descriptors)
{
}

// 析构函数：销毁描述符集对象
// 释放所有设备的实现对象
DescriptorSet::~DescriptorSet()
{
    release();
}

// 比较两个描述符集对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、描述符集布局和描述符容器
int DescriptorSet::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_pointer(setLayout, rhs.setLayout))) return result;
    return compare_pointer_container(descriptors, rhs.descriptors);
}

// 从输入流读取描述符集对象
// input: 输入流对象
// 读取描述符集布局和描述符列表
void DescriptorSet::read(Input& input)
{
    Object::read(input);

    input.readObject("setLayout", setLayout);
    input.readObjects("descriptors", descriptors);
}

// 将描述符集对象写入输出流
// output: 输出流对象
// 写入描述符集布局和描述符列表
void DescriptorSet::write(Output& output) const
{
    Object::write(output);

    output.writeObject("setLayout", setLayout);
    output.writeObjects("descriptors", descriptors);
}

// 编译描述符集
// context: 编译上下文对象
// 确保描述符集布局和所有描述符都已编译，然后分配Vulkan描述符集并分配描述符
void DescriptorSet::compile(Context& context)
{
    if (!_implementation[context.deviceID])
    {
        // 确保所有相关对象都已编译
        if (setLayout) setLayout->compile(context);
        for (auto& descriptor : descriptors) descriptor->compile(context);

        // 从上下文分配描述符集并分配描述符
        _implementation[context.deviceID] = context.allocateDescriptorSet(setLayout);
        _implementation[context.deviceID]->assign(context, descriptors);
    }
}

// 释放指定设备的描述符集实现
// deviceID: 设备ID
// 回收指定设备的实现对象
void DescriptorSet::release(uint32_t deviceID)
{
    Implementation::recycle(_implementation[deviceID]);
}

// 释放所有设备的描述符集实现
// 回收所有设备的实现对象
void DescriptorSet::release()
{
    for (auto& dsi : _implementation) Implementation::recycle(dsi);
    _implementation.clear();
}

// 获取Vulkan描述符集句柄
// deviceID: 设备ID
// 返回: Vulkan描述符集句柄
VkDescriptorSet DescriptorSet::vk(uint32_t deviceID) const
{
    return _implementation[deviceID]->_descriptorSet;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DescriptorSet::Implementation
//
// 实现类构造函数：创建描述符集实现对象
// descriptorPool: 描述符池对象（用于分配描述符集）
// descriptorSetLayout: 描述符集布局对象（定义描述符的结构）
// 从描述符池分配Vulkan描述符集
// 注意：不需要本地锁定描述符池，因为此构造函数应该只由DescriptorPool::allocateDescriptorSet调用，
// 该函数在调用此构造函数之前已经锁定了DescriptorPool::mutex
DescriptorSet::Implementation::Implementation(DescriptorPool* descriptorPool, DescriptorSetLayout* descriptorSetLayout) :
    _descriptorPool(descriptorPool),
    _descriptorSetLayout(descriptorSetLayout)
{
    auto device = descriptorPool->getDevice();

    VkDescriptorSetLayout vkdescriptorSetLayout = descriptorSetLayout->vk(device->deviceID);

    // 设置描述符集分配信息
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = *descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &vkdescriptorSetLayout;

    // 不需要本地锁定描述符池，因为DescriptorSet::Implementation构造函数应该只由
    // DescriptorPool::allocateDescriptorSet调用，该函数在调用此构造函数之前已经锁定了DescriptorPool::mutex。
    // 否则我们需要：std::scoped_lock<std::mutex> lock(_descriptorPool->mutex);

    // 分配Vulkan描述符集
    if (VkResult result = vkAllocateDescriptorSets(*device, &descriptorSetAllocateInfo, &_descriptorSet); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create DescriptorSet.", result};
    }
}

// 实现类析构函数：销毁描述符集实现对象
// 释放Vulkan描述符集回描述符池
DescriptorSet::Implementation::~Implementation()
{
    if (_descriptorPool && _descriptorSet)
    {
        std::scoped_lock<std::mutex> lock(_descriptorPool->mutex);
        vkFreeDescriptorSets(*(_descriptorPool->getDevice()), *_descriptorPool, 1, &_descriptorSet);
    }
}

// 分配描述符到描述符集
// context: 编译上下文对象
// in_descriptors: 描述符列表
// 从临时内存分配写入描述符集结构，填充所有描述符信息，然后更新Vulkan描述符集
void DescriptorSet::Implementation::assign(Context& context, const Descriptors& in_descriptors)
{
    if (in_descriptors.empty()) return;

    // 从临时内存分配写入描述符集结构数组
    VkWriteDescriptorSet* descriptorWrites = context.scratchMemory->allocate<VkWriteDescriptorSet>(in_descriptors.size());

    // 填充每个描述符的写入信息
    for (size_t i = 0; i < in_descriptors.size(); ++i)
    {
        in_descriptors[i]->assignTo(context, descriptorWrites[i]);
        descriptorWrites[i].dstSet = _descriptorSet;
    }

    // 更新Vulkan描述符集
    auto device = _descriptorPool->getDevice();
    vkUpdateDescriptorSets(*device, static_cast<uint32_t>(in_descriptors.size()), descriptorWrites, 0, nullptr);

    // 清理临时内存以便重用
    context.scratchMemory->release();
}

// 回收描述符集实现对象
// dsi: 描述符集实现对象的引用
// 将实现对象释放回描述符池，然后清空引用
void DescriptorSet::Implementation::recycle(ref_ptr<DescriptorSet::Implementation>& dsi)
{
    if (dsi)
    {
        if (dsi->_descriptorPool) dsi->_descriptorPool->freeDescriptorSet(dsi);
        dsi = {};
    }
}
