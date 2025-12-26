/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/state/Descriptor.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建描述符对象
// in_dstBinding: 目标绑定点索引（在描述符集布局中的绑定点）
// in_dstArrayElement: 目标数组元素索引（用于数组描述符）
// in_descriptorType: Vulkan描述符类型（如统一缓冲区、采样器等）
// 描述符用于将资源（缓冲区、图像、采样器等）绑定到着色器
Descriptor::Descriptor(uint32_t in_dstBinding, uint32_t in_dstArrayElement, VkDescriptorType in_descriptorType) :
    dstBinding(in_dstBinding),
    dstArrayElement(in_dstArrayElement),
    descriptorType(in_descriptorType)
{
}

// 拷贝构造函数：从另一个描述符对象创建新的描述符对象
// rhs: 要拷贝的描述符对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝绑定点、数组元素索引和描述符类型
Descriptor::Descriptor(const Descriptor& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    dstBinding(rhs.dstBinding),
    dstArrayElement(rhs.dstArrayElement),
    descriptorType(rhs.descriptorType)
{
}

// 比较两个描述符对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、目标绑定点、目标数组元素和描述符类型
int Descriptor::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(dstBinding, rhs.dstBinding))) return result;
    if ((result = compare_value(dstArrayElement, rhs.dstArrayElement))) return result;
    return compare_value(descriptorType, rhs.descriptorType);
}

// 从输入流读取描述符对象
// input: 输入流对象
// 读取目标绑定点、目标数组元素和描述符类型（版本1.1.11及以上）
void Descriptor::read(Input& input)
{
    Object::read(input);

    input.read("dstBinding", dstBinding);
    input.read("dstArrayElement", dstArrayElement);
    if (input.version_greater_equal(1, 1, 11)) input.read("descriptorType", descriptorType);
}

// 将描述符对象写入输出流
// output: 输出流对象
// 写入目标绑定点、目标数组元素和描述符类型（版本1.1.11及以上）
void Descriptor::write(Output& output) const
{
    Object::write(output);

    output.write("dstBinding", dstBinding);
    output.write("dstArrayElement", dstArrayElement);
    if (output.version_greater_equal(1, 1, 11)) output.write("descriptorType", descriptorType);
}

// 将描述符信息分配到Vulkan写入描述符集结构
// context: 编译上下文对象（未使用）
// wds: 输出参数，用于填充Vulkan写入描述符集结构
// 填充写入描述符集的基本信息（结构类型、目标绑定点、目标数组元素、描述符类型）
void Descriptor::assignTo(Context& /*context*/, VkWriteDescriptorSet& wds) const
{
    wds = {};
    wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wds.dstBinding = dstBinding;
    wds.dstArrayElement = dstArrayElement;
    wds.descriptorType = descriptorType;
}
