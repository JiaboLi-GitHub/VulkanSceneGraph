/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/io/Logger.h>
#include <vsg/state/DynamicState.h>
#include <vsg/state/GraphicsPipeline.h>
#include <vsg/state/ViewportState.h>
#include <vsg/vk/Context.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////
//
// GraphicsPipelineState
//
// 比较两个图形管线状态对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较掩码
int GraphicsPipelineState::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_value(mask, rhs.mask);
}

// 从输入流读取图形管线状态对象
// input: 输入流对象
// 读取掩码（版本1.0.9及以上）
void GraphicsPipelineState::read(Input& input)
{
    Object::read(input);

    if (input.version_greater_equal(1, 0, 9))
    {
        input.read("mask", mask);
    }
}

// 将图形管线状态对象写入输出流
// output: 输出流对象
// 写入掩码（版本1.0.9及以上）
void GraphicsPipelineState::write(Output& output) const
{
    Object::write(output);

    if (output.version_greater_equal(1, 0, 9))
    {
        output.write("mask", mask);
    }
}

void vsg::mergeGraphicsPipelineStates(Mask mask, GraphicsPipelineStates& dest_PipelineStates, ref_ptr<GraphicsPipelineState> src_PipelineState)
{
    if (!src_PipelineState || (mask & src_PipelineState->mask) == 0) return;

    auto* src_DynamicState = src_PipelineState->cast<DynamicState>();

    // replace any entries in the dest_PipelineStates that have the same type as src_PipelineState
    for (auto& original_pipelineState : dest_PipelineStates)
    {
        if (original_pipelineState->type_info() == src_PipelineState->type_info())
        {
            if (src_DynamicState)
            {
                auto original_DynamicState = original_pipelineState->cast<DynamicState>();
                if (original_DynamicState->dynamicStates.empty())
                {
                    original_pipelineState = src_PipelineState;
                }
                else if (original_DynamicState->dynamicStates != src_DynamicState->dynamicStates)
                {
                    auto new_DynamicState = DynamicState::create(original_DynamicState->dynamicStates);
                    for (auto state : src_DynamicState->dynamicStates)
                    {
                        if (std::find(new_DynamicState->dynamicStates.begin(), new_DynamicState->dynamicStates.end(), state) == new_DynamicState->dynamicStates.end())
                        {
                            new_DynamicState->dynamicStates.push_back(state);
                        }
                    }

                    original_pipelineState = new_DynamicState;
                }
            }
            else
            {
                original_pipelineState = src_PipelineState;
            }
            return;
        }
    }
    dest_PipelineStates.push_back(src_PipelineState);
}

void vsg::mergeGraphicsPipelineStates(Mask mask, GraphicsPipelineStates& dest_PipelineStates, const GraphicsPipelineStates& src_PipelineStates)
{
    for (const auto& src_PipelineState : src_PipelineStates)
    {
        mergeGraphicsPipelineStates(mask, dest_PipelineStates, src_PipelineState);
    }
}

////////////////////////////////////////////////////////////////////////
//
// GraphicsPipeline
//
// 构造函数：创建图形管线对象（默认）
// 图形管线用于定义图形渲染的完整管线状态（着色器、渲染状态等）
GraphicsPipeline::GraphicsPipeline()
{
}

// 构造函数：使用管线布局、着色器阶段、管线状态和子通道创建图形管线对象
// in_pipelineLayout: 管线布局对象（定义描述符集布局和推送常量范围）
// in_shaderStages: 着色器阶段列表（顶点、片段、几何等）
// in_pipelineStates: 管线状态列表（光栅化、深度模板、颜色混合等）
// in_subpass: 子通道索引（在渲染通道中的子通道）
GraphicsPipeline::GraphicsPipeline(PipelineLayout* in_pipelineLayout, const ShaderStages& in_shaderStages, const GraphicsPipelineStates& in_pipelineStates, uint32_t in_subpass) :
    stages(in_shaderStages),
    pipelineStates(in_pipelineStates),
    layout(in_pipelineLayout),
    subpass(in_subpass)
{
}

// 析构函数：销毁图形管线对象
GraphicsPipeline::~GraphicsPipeline()
{
}

// 比较两个图形管线对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、着色器阶段容器、管线状态容器、布局和子通道
int GraphicsPipeline::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_pointer_container(stages, rhs.stages))) return result;
    if ((result = compare_pointer_container(pipelineStates, rhs.pipelineStates))) return result;
    if ((result = compare_pointer(layout, rhs.layout))) return result;
    return compare_value(subpass, rhs.subpass);
}

// 从输入流读取图形管线对象
// input: 输入流对象
// 读取管线布局、着色器阶段、管线状态和子通道
void GraphicsPipeline::read(Input& input)
{
    Object::read(input);

    input.readObject("layout", layout);
    input.readObjects("stages", stages);
    input.readObjects("pipelineStates", pipelineStates);
    input.read("subpass", subpass);
}

// 将图形管线对象写入输出流
// output: 输出流对象
// 写入管线布局、着色器阶段、管线状态和子通道
void GraphicsPipeline::write(Output& output) const
{
    Object::write(output);

    output.writeObject("layout", layout);
    output.writeObjects("stages", stages);
    output.writeObjects("pipelineStates", pipelineStates);
    output.write("subpass", subpass);
}

// 编译图形管线
// context: 编译上下文对象
// 合并默认、本地和覆盖的管线状态，编译着色器（如果需要），然后创建Vulkan图形管线对象
// 支持视图特定的实现（每个视图可以有独立的管线状态）
void GraphicsPipeline::compile(Context& context)
{
    uint32_t viewID = context.viewID;
    // 确保实现数组足够大
    if (static_cast<uint32_t>(_implementation.size()) < (viewID + 1))
    {
        _implementation.resize(viewID + 1);
    }

    if (!_implementation[viewID])
    {
        // 合并管线状态：默认状态、本地状态、覆盖状态
        GraphicsPipelineStates combined_pipelineStates;
        combined_pipelineStates.reserve(context.defaultPipelineStates.size() + pipelineStates.size() + context.overridePipelineStates.size());
        mergeGraphicsPipelineStates(context.mask, combined_pipelineStates, context.defaultPipelineStates);
        mergeGraphicsPipelineStates(context.mask, combined_pipelineStates, pipelineStates);
        mergeGraphicsPipelineStates(context.mask, combined_pipelineStates, context.overridePipelineStates);

        // 检查是否已有相同状态的实现可以重用
        for (const auto& imp : _implementation)
        {
            if (imp && vsg::compare_pointer_container(imp->_pipelineStates, combined_pipelineStates) == 0)
            {
                _implementation[viewID] = imp;
                return;
            }
        }

        // 如果需要，编译着色器（从源代码编译为SPIR-V）
        bool requiresShaderCompiler = false;
        for (const auto& shaderStage : stages)
        {
            if (shaderStage->module)
            {
                if (shaderStage->module->code.empty() && !(shaderStage->module->source.empty()))
                {
                    requiresShaderCompiler = true;
                }
            }
        }

        if (requiresShaderCompiler)
        {
            auto shaderCompiler = context.getOrCreateShaderCompiler();
            if (shaderCompiler)
            {
                shaderCompiler->compile(stages); // 可能需要以某种方式映射定义和路径
            }
            else
            {
                fatal("VulkanSceneGraph not compiled with GLSLang, unable to compile shaders.");
            }
        }

        // 编译Vulkan对象
        layout->compile(context);

        // 编译所有着色器阶段
        for (auto& shaderStage : stages)
        {
            shaderStage->compile(context);
        }

        // 创建图形管线实现
        _implementation[viewID] = GraphicsPipeline::Implementation::create(context, context.device, context.renderPass, layout, stages, combined_pipelineStates, subpass);
    }
}

////////////////////////////////////////////////////////////////////////
//
// GraphicsPipeline::Implementation
//
GraphicsPipeline::Implementation::Implementation(Context& context, Device* device, const RenderPass* renderPass, const PipelineLayout* pipelineLayout, const ShaderStages& shaderStages, const GraphicsPipelineStates& pipelineStates, uint32_t subpass) :
    _pipelineStates(pipelineStates),
    _device(device)
{
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout->vk(device->deviceID);
    pipelineInfo.renderPass = *renderPass;
    pipelineInfo.subpass = subpass;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.pNext = nullptr;

    auto shaderStageCreateInfo = context.scratchMemory->allocate<VkPipelineShaderStageCreateInfo>(shaderStages.size());
    uint32_t i = 0;
    for (auto& shaderStage : shaderStages)
    {
        // check if ShaderStage is appropriate to assign to stageInfo
        if ((context.mask & shaderStage->mask) != 0)
        {
            shaderStageCreateInfo[i].flags = 0;
            shaderStageCreateInfo[i].pNext = nullptr;
            shaderStage->apply(context, shaderStageCreateInfo[i]);
            ++i;
        }
    }

    pipelineInfo.stageCount = i;
    pipelineInfo.pStages = shaderStageCreateInfo;

    for (auto pipelineState : pipelineStates)
    {
        pipelineState->apply(context, pipelineInfo);
    }

    VkResult result = vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &pipelineInfo, _device->getAllocationCallbacks(), &_pipeline);

    context.scratchMemory->release();

    if (result != VK_SUCCESS)
    {
        throw Exception{"Error: vsg::GraphicsPipeline failed to create VkPipeline.", result};
    }
}

GraphicsPipeline::Implementation::~Implementation()
{
    vkDestroyPipeline(*_device, _pipeline, _device->getAllocationCallbacks());
}

////////////////////////////////////////////////////////////////////////
//
// BindGraphicsPipeline
//
BindGraphicsPipeline::BindGraphicsPipeline(ref_ptr<GraphicsPipeline> in_pipeline) :
    Inherit(0), // slot 0
    pipeline(in_pipeline)
{
}

BindGraphicsPipeline::~BindGraphicsPipeline()
{
}

int BindGraphicsPipeline::compare(const Object& rhs_object) const
{
    int result = StateCommand::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_pointer(pipeline, rhs.pipeline);
}

void BindGraphicsPipeline::read(Input& input)
{
    StateCommand::read(input);

    input.readObject("pipeline", pipeline);
}

void BindGraphicsPipeline::write(Output& output) const
{
    StateCommand::write(output);

    output.writeObject("pipeline", pipeline);
}

void BindGraphicsPipeline::record(CommandBuffer& commandBuffer) const
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->vk(commandBuffer.viewID));
    commandBuffer.setCurrentPipelineLayout(pipeline->layout);
}

void BindGraphicsPipeline::compile(Context& context)
{
    if (pipeline) pipeline->compile(context);
}

void BindGraphicsPipeline::release()
{
    if (pipeline) pipeline->release();
}
