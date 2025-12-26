/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/state/DescriptorTexelBufferView.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建描述符Texel缓冲区视图对象（默认）
// 描述符Texel缓冲区视图用于将格式化的缓冲区视图绑定到描述符集（统一texel缓冲区、存储texel缓冲区等）
// 默认类型为统一texel缓冲区
DescriptorTexelBufferView::DescriptorTexelBufferView() :
    Inherit(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
{
}

// 拷贝构造函数：从另一个描述符Texel缓冲区视图对象创建新的描述符Texel缓冲区视图对象
// rhs: 要拷贝的描述符Texel缓冲区视图对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝Texel缓冲区视图列表
DescriptorTexelBufferView::DescriptorTexelBufferView(const DescriptorTexelBufferView& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    texelBufferViews(copyop(rhs.texelBufferViews))
{
}

// 构造函数：使用绑定点、数组元素、描述符类型和Texel缓冲区视图列表创建描述符Texel缓冲区视图对象
// in_dstBinding: 目标绑定点索引
// in_dstArrayElement: 目标数组元素索引
// in_descriptorType: 描述符类型（统一texel缓冲区、存储texel缓冲区等）
// in_texelBufferViews: Texel缓冲区视图列表
DescriptorTexelBufferView::DescriptorTexelBufferView(uint32_t in_dstBinding, uint32_t in_dstArrayElement, VkDescriptorType in_descriptorType, const BufferViewList& in_texelBufferViews) :
    Inherit(in_dstBinding, in_dstArrayElement, in_descriptorType),
    texelBufferViews(in_texelBufferViews)
{
}

// 比较两个描述符Texel缓冲区视图对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较Texel缓冲区视图容器
int DescriptorTexelBufferView::compare(const Object& rhs_object) const
{
    int result = Descriptor::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_pointer_container(texelBufferViews, rhs.texelBufferViews);
}

// 从输入流读取描述符Texel缓冲区视图对象
// input: 输入流对象
// 读取Texel缓冲区视图列表
void DescriptorTexelBufferView::read(Input& input)
{
    Descriptor::read(input);

    input.readObjects("texelBufferViews", texelBufferViews);
}

// 将描述符Texel缓冲区视图对象写入输出流
// output: 输出流对象
// 写入Texel缓冲区视图列表
void DescriptorTexelBufferView::write(Output& output) const
{
    Descriptor::write(output);

    output.writeObjects("texelBufferViews", texelBufferViews);
}

// 编译描述符Texel缓冲区视图
// context: 编译上下文对象
// 编译所有Texel缓冲区视图
void DescriptorTexelBufferView::compile(Context& context)
{
    if (texelBufferViews.empty()) return;

    for (auto& bufferView : texelBufferViews)
    {
        bufferView->compile(context);
    }
}

// 将描述符Texel缓冲区视图信息分配到Vulkan写入描述符集结构
// context: 编译上下文对象
// wds: 输出参数，用于填充Vulkan写入描述符集结构
// 从临时内存分配缓冲区视图句柄数组，填充所有Texel缓冲区视图的Vulkan句柄
void DescriptorTexelBufferView::assignTo(Context& context, VkWriteDescriptorSet& wds) const
{
    Descriptor::assignTo(context, wds);

    // 从临时内存分配缓冲区视图句柄数组
    auto vk_texelBufferViews = context.scratchMemory->allocate<VkBufferView>(texelBufferViews.size());
    wds.descriptorCount = static_cast<uint32_t>(texelBufferViews.size());
    wds.pTexelBufferView = vk_texelBufferViews;

    // 编译并获取所有Texel缓冲区视图的Vulkan句柄
    for (size_t i = 0; i < texelBufferViews.size(); ++i)
    {
        texelBufferViews[i]->compile(context);
        vk_texelBufferViews[i] = texelBufferViews[i]->vk(context.deviceID);
    }
}
