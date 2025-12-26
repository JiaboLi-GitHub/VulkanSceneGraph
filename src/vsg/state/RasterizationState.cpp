/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/state/RasterizationState.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：创建光栅化状态对象（默认）
// 光栅化状态用于定义光栅化阶段的参数（深度钳制、多边形模式、面剔除、深度偏移、线宽等）
RasterizationState::RasterizationState()
{
}

// 拷贝构造函数：从另一个光栅化状态对象创建新的光栅化状态对象
// rs: 要拷贝的光栅化状态对象
// 拷贝所有光栅化参数（深度钳制、光栅化丢弃、多边形模式、剔除模式、正面、深度偏移、线宽等）
RasterizationState::RasterizationState(const RasterizationState& rs) :
    Inherit(rs),
    depthClampEnable(rs.depthClampEnable),
    rasterizerDiscardEnable(rs.rasterizerDiscardEnable),
    polygonMode(rs.polygonMode),
    cullMode(rs.cullMode),
    frontFace(rs.frontFace),
    depthBiasEnable(rs.depthBiasEnable),
    depthBiasConstantFactor(rs.depthBiasConstantFactor),
    depthBiasClamp(rs.depthBiasClamp),
    depthBiasSlopeFactor(rs.depthBiasSlopeFactor),
    lineWidth(rs.lineWidth)
{
}

// 析构函数：销毁光栅化状态对象
RasterizationState::~RasterizationState()
{
}

// 比较两个光栅化状态对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较光栅化参数区域（从depthClampEnable到lineWidth）
int RasterizationState::compare(const Object& rhs_object) const
{
    int result = GraphicsPipelineState::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_region(depthClampEnable, lineWidth, rhs.depthClampEnable);
}

// 从输入流读取光栅化状态对象
// input: 输入流对象
// 读取所有光栅化参数（深度钳制、光栅化丢弃、多边形模式、剔除模式、正面、深度偏移、线宽等）
// 注意：depthBiasConstantFactor在版本1.1.11及以上作为浮点数读取，否则作为整数读取
void RasterizationState::read(Input& input)
{
    GraphicsPipelineState::read(input);

    input.readValue<uint32_t>("depthClampEnable", depthClampEnable);
    input.readValue<uint32_t>("rasterizerDiscardEnable", rasterizerDiscardEnable);
    input.readValue<uint32_t>("polygonMode", polygonMode);
    input.readValue<uint32_t>("cullMode", cullMode);
    input.readValue<uint32_t>("frontFace", frontFace);
    input.readValue<uint32_t>("depthBiasEnable", depthBiasEnable);

    if (input.version_greater_equal(1, 1, 11))
        input.read("depthBiasConstantFactor", depthBiasConstantFactor);
    else
        input.readValue<uint32_t>("depthBiasConstantFactor", depthBiasConstantFactor);

    input.read("depthBiasClamp", depthBiasClamp);
    input.read("depthBiasSlopeFactor", depthBiasSlopeFactor);
    input.read("lineWidth", lineWidth);
}

// 将光栅化状态对象写入输出流
// output: 输出流对象
// 写入所有光栅化参数
void RasterizationState::write(Output& output) const
{
    GraphicsPipelineState::write(output);

    output.writeValue<uint32_t>("depthClampEnable", depthClampEnable);
    output.writeValue<uint32_t>("rasterizerDiscardEnable", rasterizerDiscardEnable);
    output.writeValue<uint32_t>("polygonMode", polygonMode);
    output.writeValue<uint32_t>("cullMode", cullMode);
    output.writeValue<uint32_t>("frontFace", frontFace);
    output.writeValue<uint32_t>("depthBiasEnable", depthBiasEnable);

    if (output.version_greater_equal(1, 1, 11))
        output.write("depthBiasConstantFactor", depthBiasConstantFactor);
    else
        output.writeValue<uint32_t>("depthBiasConstantFactor", depthBiasConstantFactor);

    output.write("depthBiasClamp", depthBiasClamp);
    output.write("depthBiasSlopeFactor", depthBiasSlopeFactor);
    output.write("lineWidth", lineWidth);
}

// 应用光栅化状态到图形管线创建信息
// context: 编译上下文对象
// pipelineInfo: 图形管线创建信息（输出参数）
// 从临时内存分配光栅化状态创建信息，填充所有光栅化参数，然后设置到管线创建信息中
void RasterizationState::apply(Context& context, VkGraphicsPipelineCreateInfo& pipelineInfo) const
{
    auto rasteratizationState = context.scratchMemory->allocate<VkPipelineRasterizationStateCreateInfo>();

    rasteratizationState->sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasteratizationState->pNext = nullptr;
    rasteratizationState->flags = 0;

    rasteratizationState->depthClampEnable = depthClampEnable;
    rasteratizationState->rasterizerDiscardEnable = rasterizerDiscardEnable;
    rasteratizationState->polygonMode = polygonMode;
    rasteratizationState->cullMode = cullMode;
    rasteratizationState->frontFace = frontFace;
    rasteratizationState->depthBiasEnable = depthBiasEnable;
    rasteratizationState->depthBiasConstantFactor = depthBiasConstantFactor;
    rasteratizationState->depthBiasClamp = depthBiasClamp;
    rasteratizationState->depthBiasSlopeFactor = depthBiasSlopeFactor;
    rasteratizationState->lineWidth = lineWidth;

    pipelineInfo.pRasterizationState = rasteratizationState;
}
