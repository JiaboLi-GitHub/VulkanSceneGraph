/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/state/TessellationState.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：使用补丁控制点数量创建曲面细分状态对象
// in_patchControlPoints: 补丁控制点数量（每个补丁的顶点数，通常为3或4）
// 曲面细分状态用于定义曲面细分阶段的参数
TessellationState::TessellationState(uint32_t in_patchControlPoints) :
    patchControlPoints(in_patchControlPoints)
{
}

// 拷贝构造函数：从另一个曲面细分状态对象创建新的曲面细分状态对象
// ts: 要拷贝的曲面细分状态对象
// 拷贝补丁控制点数量
TessellationState::TessellationState(const TessellationState& ts) :
    Inherit(ts),
    patchControlPoints(ts.patchControlPoints)
{
}

// 析构函数：销毁曲面细分状态对象
TessellationState::~TessellationState()
{
}

// 比较两个曲面细分状态对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类和补丁控制点数量
int TessellationState::compare(const Object& rhs_object) const
{
    int result = GraphicsPipelineState::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_value(patchControlPoints, rhs.patchControlPoints);
}

// 从输入流读取曲面细分状态对象
// input: 输入流对象
// 读取补丁控制点数量
void TessellationState::read(Input& input)
{
    GraphicsPipelineState::read(input);

    input.read("patchControlPoints", patchControlPoints);
}

// 将曲面细分状态对象写入输出流
// output: 输出流对象
// 写入补丁控制点数量
void TessellationState::write(Output& output) const
{
    GraphicsPipelineState::write(output);

    output.write("patchControlPoints", patchControlPoints);
}

// 应用曲面细分状态到图形管线创建信息
// context: 编译上下文对象
// pipelineInfo: 图形管线创建信息（输出参数）
// 从临时内存分配曲面细分状态创建信息，填充补丁控制点数量，然后设置到管线创建信息中
void TessellationState::apply(Context& context, VkGraphicsPipelineCreateInfo& pipelineInfo) const
{

    auto tessellationState = context.scratchMemory->allocate<VkPipelineTessellationStateCreateInfo>();

    tessellationState->sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellationState->pNext = nullptr;
    tessellationState->flags = 0;
    tessellationState->patchControlPoints = patchControlPoints;

    pipelineInfo.pTessellationState = tessellationState;
}
