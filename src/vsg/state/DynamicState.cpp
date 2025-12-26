/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/state/DynamicState.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建动态状态对象（默认）
// 动态状态用于定义可以在命令缓冲区中动态设置的状态（如视口、裁剪矩形、线宽、深度偏移等）
DynamicState::DynamicState()
{
}

// 拷贝构造函数：从另一个动态状态对象创建新的动态状态对象
// ds: 要拷贝的动态状态对象
// 拷贝动态状态列表
DynamicState::DynamicState(const DynamicState& ds) :
    Inherit(ds),
    dynamicStates(ds.dynamicStates)
{
}

// 析构函数：销毁动态状态对象
DynamicState::~DynamicState()
{
}

// 比较两个动态状态对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较动态状态容器
int DynamicState::compare(const Object& rhs_object) const
{
    int result = GraphicsPipelineState::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_value_container(dynamicStates, rhs.dynamicStates);
}

// 从输入流读取动态状态对象
// input: 输入流对象
// 读取动态状态列表
void DynamicState::read(Input& input)
{
    GraphicsPipelineState::read(input);

    dynamicStates.resize(input.readValue<uint32_t>("NumDynamicStates"));
    for (auto& dynamicState : dynamicStates)
    {
        input.readValue<uint32_t>("value", dynamicState);
    }
}

// 将动态状态对象写入输出流
// output: 输出流对象
// 写入动态状态列表
void DynamicState::write(Output& output) const
{
    GraphicsPipelineState::write(output);

    output.writeValue<uint32_t>("NumDynamicStates", dynamicStates.size());
    for (auto& dynamicState : dynamicStates)
    {
        output.writeValue<uint32_t>("value", dynamicState);
    }
}

// 应用动态状态到图形管线创建信息
// context: 编译上下文对象
// pipelineInfo: 图形管线创建信息（输出参数）
// 从临时内存分配动态状态创建信息，填充动态状态列表，然后设置到管线创建信息中
void DynamicState::apply(Context& context, VkGraphicsPipelineCreateInfo& pipelineInfo) const
{
    auto dynamicState = context.scratchMemory->allocate<VkPipelineDynamicStateCreateInfo>();

    dynamicState->sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState->pNext = nullptr;
    dynamicState->flags = 0;
    dynamicState->dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState->pDynamicStates = dynamicStates.data();

    pipelineInfo.pDynamicState = dynamicState;
}
