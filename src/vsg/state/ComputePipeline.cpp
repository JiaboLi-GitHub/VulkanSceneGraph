/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/io/Logger.h>
#include <vsg/state/ComputePipeline.h>
#include <vsg/vk/Context.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////
//
// ComputePipeline
//
// 构造函数：创建计算管线对象（默认）
// 计算管线用于定义计算着色器的管线状态
ComputePipeline::ComputePipeline()
{
}

// 构造函数：使用管线布局和着色器阶段创建计算管线对象
// pipelineLayout: 管线布局对象（定义描述符集布局和推送常量范围）
// shaderStage: 计算着色器阶段对象
ComputePipeline::ComputePipeline(PipelineLayout* pipelineLayout, ShaderStage* shaderStage) :
    layout(pipelineLayout),
    stage(shaderStage)
{
}

// 析构函数：销毁计算管线对象
ComputePipeline::~ComputePipeline()
{
}

// 比较两个计算管线对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、管线布局和着色器阶段
int ComputePipeline::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_pointer(layout, rhs.layout))) return result;
    return compare_pointer(stage, rhs.stage);
}

// 从输入流读取计算管线对象
// input: 输入流对象
// 读取管线布局和着色器阶段
void ComputePipeline::read(Input& input)
{
    Object::read(input);

    input.readObject("layout", layout);
    input.readObject("stage", stage);
}

// 将计算管线对象写入输出流
// output: 输出流对象
// 写入管线布局和着色器阶段
void ComputePipeline::write(Output& output) const
{
    Object::write(output);

    output.writeObject("layout", layout);
    output.writeObject("stage", stage);
}

// 编译计算管线
// context: 编译上下文对象
// 如果需要，编译着色器（从源代码编译为SPIR-V），然后编译管线布局和着色器阶段，最后创建Vulkan计算管线对象
void ComputePipeline::compile(Context& context)
{
    if (!_implementation[context.deviceID])
    {
        // 如果需要，编译着色器（从源代码编译为SPIR-V）
        bool requiresShaderCompiler = stage && stage->module && stage->module->code.empty() && !(stage->module->source.empty());

        if (requiresShaderCompiler)
        {
            auto shaderCompiler = context.getOrCreateShaderCompiler();
            if (shaderCompiler)
            {
                shaderCompiler->compile(stage); // 可能需要以某种方式映射定义和路径
            }
            else
            {
                fatal("VulkanSceneGraph not compiled with GLSLang, unable to compile shaders.");
            }
        }

        // 编译管线布局和着色器阶段
        layout->compile(context);
        stage->compile(context);
        // 创建计算管线实现
        _implementation[context.deviceID] = ComputePipeline::Implementation::create(context, context.device, layout, stage);
    }
}

////////////////////////////////////////////////////////////////////////
//
// ComputePipeline::Implementation
//
ComputePipeline::Implementation::Implementation(Context& context, Device* device, const PipelineLayout* pipelineLayout, const ShaderStage* shaderStage) :
    _device(device)
{
    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.pNext = nullptr;
    shaderStage->apply(context, stageInfo);

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout->vk(device->deviceID);
    pipelineInfo.stage = stageInfo;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.pNext = nullptr;

    if (VkResult result = vkCreateComputePipelines(*device, VK_NULL_HANDLE, 1, &pipelineInfo, _device->getAllocationCallbacks(), &_pipeline); result != VK_SUCCESS)
    {
        throw Exception{"Error: vsg::ComputePipeline failed to create VkPipeline.", result};
    }
}

ComputePipeline::Implementation::~Implementation()
{
    vkDestroyPipeline(*_device, _pipeline, _device->getAllocationCallbacks());
}

////////////////////////////////////////////////////////////////////////
//
// BindComputePipeline
//
// 构造函数：创建绑定计算管线命令
// in_pipeline: 要绑定的计算管线对象
// 槽位0：在绑定描述符集之前执行
BindComputePipeline::BindComputePipeline(ComputePipeline* in_pipeline) :
    Inherit(0), // 槽位0
    pipeline(in_pipeline)
{
}

// 析构函数：销毁绑定计算管线命令
BindComputePipeline::~BindComputePipeline()
{
}

// 比较两个绑定计算管线命令对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类，然后比较管线对象
int BindComputePipeline::compare(const Object& rhs_object) const
{
    int result = StateCommand::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_pointer(pipeline, rhs.pipeline);
}

// 从输入流读取绑定计算管线命令对象
// input: 输入流对象
// 读取管线对象
void BindComputePipeline::read(Input& input)
{
    StateCommand::read(input);

    input.readObject("pipeline", pipeline);
}

// 将绑定计算管线命令对象写入输出流
// output: 输出流对象
// 写入管线对象
void BindComputePipeline::write(Output& output) const
{
    StateCommand::write(output);

    output.writeObject("pipeline", pipeline);
}

// 记录绑定计算管线命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdBindPipeline命令，绑定计算管线，并设置当前管线布局
void BindComputePipeline::record(CommandBuffer& commandBuffer) const
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->vk(commandBuffer.deviceID));
    commandBuffer.setCurrentPipelineLayout(pipeline->layout);
}

// 编译绑定计算管线命令
// context: 编译上下文对象
// 编译计算管线对象
void BindComputePipeline::compile(Context& context)
{
    if (pipeline) pipeline->compile(context);
}
