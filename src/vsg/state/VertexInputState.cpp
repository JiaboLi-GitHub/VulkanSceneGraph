/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/state/VertexInputState.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建顶点输入状态对象（默认）
// 顶点输入状态用于定义顶点数据的绑定和属性描述（绑定点、步长、输入速率、属性位置、格式、偏移等）
VertexInputState::VertexInputState()
{
}

// 拷贝构造函数：从另一个顶点输入状态对象创建新的顶点输入状态对象
// vis: 要拷贝的顶点输入状态对象
// 拷贝顶点绑定描述列表和顶点属性描述列表
VertexInputState::VertexInputState(const VertexInputState& vis) :
    Inherit(vis),
    vertexBindingDescriptions(vis.vertexBindingDescriptions),
    vertexAttributeDescriptions(vis.vertexAttributeDescriptions)
{
}

// 构造函数：使用绑定描述和属性描述创建顶点输入状态对象
// bindings: 顶点绑定描述列表（定义顶点缓冲区的绑定方式）
// attributes: 顶点属性描述列表（定义顶点属性的位置、格式、偏移等）
VertexInputState::VertexInputState(const Bindings& bindings, const Attributes& attributes) :
    vertexBindingDescriptions(bindings),
    vertexAttributeDescriptions(attributes)
{
}

// 析构函数：销毁顶点输入状态对象
VertexInputState::~VertexInputState()
{
}

// 比较两个顶点输入状态对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、顶点绑定描述容器和顶点属性描述容器
int VertexInputState::compare(const Object& rhs_object) const
{
    int result = GraphicsPipelineState::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value_container(vertexBindingDescriptions, rhs.vertexBindingDescriptions))) return result;
    return compare_value_container(vertexAttributeDescriptions, rhs.vertexAttributeDescriptions);
}

// 从输入流读取顶点输入状态对象
// input: 输入流对象
// 读取顶点绑定描述（绑定点、步长、输入速率）和顶点属性描述（位置、绑定点、格式、偏移）
void VertexInputState::read(Input& input)
{
    GraphicsPipelineState::read(input);

    vertexBindingDescriptions.resize(input.readValue<uint32_t>("NumBindings"));
    for (auto& binding : vertexBindingDescriptions)
    {
        input.read("binding", binding.binding);
        input.read("stride", binding.stride);
        input.readValue<uint32_t>("inputRate", binding.inputRate);
    }

    vertexAttributeDescriptions.resize(input.readValue<uint32_t>("NumAttributes"));
    for (auto& attribute : vertexAttributeDescriptions)
    {
        input.read("location", attribute.location);
        input.read("binding", attribute.binding);
        input.readValue<uint32_t>("format", attribute.format);
        input.read("offset", attribute.offset);
    }
}

// 将顶点输入状态对象写入输出流
// output: 输出流对象
// 写入顶点绑定描述和顶点属性描述
void VertexInputState::write(Output& output) const
{
    GraphicsPipelineState::write(output);

    output.writeValue<uint32_t>("NumBindings", vertexBindingDescriptions.size());
    for (auto& binding : vertexBindingDescriptions)
    {
        output.write("binding", binding.binding);
        output.write("stride", binding.stride);
        output.writeValue<uint32_t>("inputRate", binding.inputRate);
    }

    output.writeValue<uint32_t>("NumAttributes", vertexAttributeDescriptions.size());
    for (auto& attribute : vertexAttributeDescriptions)
    {
        output.write("location", attribute.location);
        output.write("binding", attribute.binding);
        output.writeValue<uint32_t>("format", attribute.format);
        output.write("offset", attribute.offset);
    }
}

// 应用顶点输入状态到图形管线创建信息
// context: 编译上下文对象
// pipelineInfo: 图形管线创建信息（输出参数）
// 从临时内存分配顶点输入状态创建信息，填充绑定和属性描述，然后设置到管线创建信息中
void VertexInputState::apply(Context& context, VkGraphicsPipelineCreateInfo& pipelineInfo) const
{
    auto vertexInputState = context.scratchMemory->allocate<VkPipelineVertexInputStateCreateInfo>();

    vertexInputState->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState->pNext = nullptr;
    vertexInputState->flags = 0;
    vertexInputState->vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDescriptions.size());
    vertexInputState->pVertexBindingDescriptions = vertexBindingDescriptions.data();
    vertexInputState->vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
    vertexInputState->pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

    pipelineInfo.pVertexInputState = vertexInputState;
}
