/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/state/InputAssemblyState.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建输入装配状态对象（默认）
// 输入装配状态用于定义图元拓扑类型和原始重启功能
InputAssemblyState::InputAssemblyState()
{
}

// 拷贝构造函数：从另一个输入装配状态对象创建新的输入装配状态对象
// ias: 要拷贝的输入装配状态对象
// 拷贝图元拓扑类型和原始重启启用标志
InputAssemblyState::InputAssemblyState(const InputAssemblyState& ias) :
    Inherit(ias),
    topology(ias.topology),
    primitiveRestartEnable(ias.primitiveRestartEnable)
{
}

// 构造函数：使用图元拓扑类型和原始重启标志创建输入装配状态对象
// primitiveTopology: 图元拓扑类型（点、线、三角形、条带等）
// primitiveRestart: 原始重启启用标志（是否允许在索引缓冲区中使用特殊索引值重启图元）
InputAssemblyState::InputAssemblyState(VkPrimitiveTopology primitiveTopology, VkBool32 primitiveRestart) :
    topology(primitiveTopology),
    primitiveRestartEnable(primitiveRestart)
{
}

// 析构函数：销毁输入装配状态对象
InputAssemblyState::~InputAssemblyState()
{
}

// 比较两个输入装配状态对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、图元拓扑类型和原始重启启用标志
int InputAssemblyState::compare(const Object& rhs_object) const
{
    int result = GraphicsPipelineState::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(topology, rhs.topology))) return result;
    return compare_value(primitiveRestartEnable, rhs.primitiveRestartEnable);
}

// 从输入流读取输入装配状态对象
// input: 输入流对象
// 读取图元拓扑类型和原始重启启用标志
void InputAssemblyState::read(Input& input)
{
    GraphicsPipelineState::read(input);

    input.readValue<uint32_t>("topology", topology);
    primitiveRestartEnable = input.readValue<uint32_t>("primitiveRestartEnable") != 0;
}

// 将输入装配状态对象写入输出流
// output: 输出流对象
// 写入图元拓扑类型和原始重启启用标志
void InputAssemblyState::write(Output& output) const
{
    GraphicsPipelineState::write(output);

    output.writeValue<uint32_t>("topology", topology);
    output.writeValue<uint32_t>("primitiveRestartEnable", primitiveRestartEnable ? 1 : 0);
}

// 应用输入装配状态到图形管线创建信息
// context: 编译上下文对象
// pipelineInfo: 图形管线创建信息（输出参数）
// 从临时内存分配输入装配状态创建信息，填充图元拓扑类型和原始重启启用标志，然后设置到管线创建信息中
void InputAssemblyState::apply(Context& context, VkGraphicsPipelineCreateInfo& pipelineInfo) const
{
    auto inputAssemblyState = context.scratchMemory->allocate<VkPipelineInputAssemblyStateCreateInfo>();

    inputAssemblyState->sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState->pNext = nullptr;
    inputAssemblyState->flags = 0;
    inputAssemblyState->topology = topology;
    inputAssemblyState->primitiveRestartEnable = primitiveRestartEnable;

    pipelineInfo.pInputAssemblyState = inputAssemblyState;
}
