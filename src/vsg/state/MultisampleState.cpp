/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/state/MultisampleState.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 构造函数：使用采样数创建多采样状态对象
// samples: 光栅化采样数（1、2、4、8等）
// 多采样状态用于定义多重采样抗锯齿的参数
MultisampleState::MultisampleState(VkSampleCountFlagBits samples) :
    rasterizationSamples(samples)
{
}

// 拷贝构造函数：从另一个多采样状态对象创建新的多采样状态对象
// ms: 要拷贝的多采样状态对象
// 拷贝所有多采样参数（光栅化采样数、采样着色启用、最小采样着色、采样掩码、Alpha到覆盖、Alpha到一等）
MultisampleState::MultisampleState(const MultisampleState& ms) :
    Inherit(ms),
    rasterizationSamples(ms.rasterizationSamples),
    sampleShadingEnable(ms.sampleShadingEnable),
    minSampleShading(ms.minSampleShading),
    sampleMasks(ms.sampleMasks),
    alphaToCoverageEnable(ms.alphaToCoverageEnable),
    alphaToOneEnable(ms.alphaToOneEnable)
{
}

// 析构函数：销毁多采样状态对象
MultisampleState::~MultisampleState()
{
}

// 比较两个多采样状态对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、光栅化采样数、采样着色启用、最小采样着色、采样掩码容器、Alpha到覆盖启用和Alpha到一启用
int MultisampleState::compare(const Object& rhs_object) const
{
    int result = GraphicsPipelineState::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(rasterizationSamples, rhs.rasterizationSamples))) return result;
    if ((result = compare_value(sampleShadingEnable, rhs.sampleShadingEnable))) return result;
    if ((result = compare_value(minSampleShading, rhs.minSampleShading))) return result;
    if ((result = compare_value_container(sampleMasks, rhs.sampleMasks))) return result;
    if ((result = compare_value(alphaToCoverageEnable, rhs.alphaToCoverageEnable))) return result;
    return compare_value(alphaToOneEnable, rhs.alphaToOneEnable);
}

void MultisampleState::read(Input& input)
{
    GraphicsPipelineState::read(input);

    input.readValue<uint32_t>("rasterizationSamples", rasterizationSamples);
    input.readValue<uint32_t>("sampleShadingEnable", sampleShadingEnable);
    input.read("minSampleShading", minSampleShading);

    if (input.version_greater_equal(0, 7, 3))
        sampleMasks.resize(input.readValue<uint32_t>("sampleMasks"));
    else
        sampleMasks.resize(input.readValue<uint32_t>("NumSampleMask"));

    for (auto& value : sampleMasks)
    {
        input.readValue<uint32_t>("value", value);
    }

    input.readValue<uint32_t>("alphaToCoverageEnable", alphaToCoverageEnable);
    input.readValue<uint32_t>("alphaToOneEnable", alphaToOneEnable);
}

void MultisampleState::write(Output& output) const
{
    GraphicsPipelineState::write(output);

    output.writeValue<uint32_t>("rasterizationSamples", rasterizationSamples);
    output.writeValue<uint32_t>("sampleShadingEnable", sampleShadingEnable);
    output.write("minSampleShading", minSampleShading);

    if (output.version_greater_equal(0, 7, 3))
        output.writeValue<uint32_t>("sampleMasks", sampleMasks.size());
    else
        output.writeValue<uint32_t>("NumSampleMask", sampleMasks.size());

    for (auto& value : sampleMasks)
    {
        output.writeValue<uint32_t>("value", value);
    }

    output.writeValue<uint32_t>("alphaToCoverageEnable", alphaToCoverageEnable);
    output.writeValue<uint32_t>("alphaToOneEnable", alphaToOneEnable);
}

// 应用多采样状态到图形管线创建信息
// context: 编译上下文对象
// pipelineInfo: 图形管线创建信息（输出参数）
// 从临时内存分配多采样状态创建信息，填充所有多采样参数，然后设置到管线创建信息中
void MultisampleState::apply(Context& context, VkGraphicsPipelineCreateInfo& pipelineInfo) const
{
    auto multisampleState = context.scratchMemory->allocate<VkPipelineMultisampleStateCreateInfo>();

    multisampleState->sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState->pNext = nullptr;
    multisampleState->flags = 0;

    multisampleState->rasterizationSamples = rasterizationSamples;
    multisampleState->sampleShadingEnable = sampleShadingEnable;
    multisampleState->minSampleShading = minSampleShading;
    multisampleState->pSampleMask = sampleMasks.empty() ? nullptr : sampleMasks.data();
    multisampleState->alphaToCoverageEnable = alphaToCoverageEnable;
    multisampleState->alphaToOneEnable = alphaToOneEnable;

    pipelineInfo.pMultisampleState = multisampleState;
}
