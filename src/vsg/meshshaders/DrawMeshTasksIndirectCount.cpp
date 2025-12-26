/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/meshshaders/DrawMeshTasksIndirectCount.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// DrawMeshTasksIndirectCount类的默认构造函数
// 创建带计数缓冲区的间接网格着色器绘制任务命令
// 绘制次数从单独的计数缓冲区读取，支持动态调整绘制数量
DrawMeshTasksIndirectCount::DrawMeshTasksIndirectCount() :
    drawParameters(BufferInfo::create()),  // 绘制参数缓冲区
    drawCount(BufferInfo::create())  // 绘制计数缓冲区
{
}

// DrawMeshTasksIndirectCount类的构造函数
// 从数据对象创建带计数缓冲区的间接绘制命令
// in_drawParametersData: 包含绘制参数的数据对象
// in_drawCountData: 包含绘制计数的数据对象
// in_maxDrawCount: 最大绘制次数
// in_stride: 绘制参数之间的步长（字节）
DrawMeshTasksIndirectCount::DrawMeshTasksIndirectCount(ref_ptr<Data> in_drawParametersData, ref_ptr<Data> in_drawCountData, uint32_t in_maxDrawCount, uint32_t in_stride) :
    drawParameters(BufferInfo::create(in_drawParametersData)),
    drawCount(BufferInfo::create(in_drawCountData)),
    maxDrawCount(in_maxDrawCount),
    stride(in_stride)
{
}

// 从输入流读取DrawMeshTasksIndirectCount对象
// 读取绘制参数缓冲区、计数缓冲区和相关参数
void DrawMeshTasksIndirectCount::read(Input& input)
{
    // 读取绘制参数缓冲区
    input.readObject("drawParameters.data", drawParameters->data);
    if (!drawParameters->data)
    {
        input.read("drawParameters.buffer", drawParameters->buffer);
        input.readValue<uint32_t>("drawParameters.offset", drawParameters->offset);
        input.readValue<uint32_t>("drawParameters.range", drawParameters->range);
    }

    // 读取绘制计数缓冲区
    input.readObject("drawCount.data", drawCount->data);
    if (!drawCount->data)
    {
        input.read("drawCount.buffer", drawCount->buffer);
        input.readValue<uint32_t>("drawCount.offset", drawCount->offset);
        input.readValue<uint32_t>("drawCount.range", drawCount->range);
    }

    // 读取最大绘制次数和步长
    input.read("maxDrawCount", maxDrawCount);
    input.read("stride", stride);
}

// 将DrawMeshTasksIndirectCount对象写入输出流
// 写入绘制参数缓冲区、计数缓冲区和相关参数
void DrawMeshTasksIndirectCount::write(Output& output) const
{
    // 写入绘制参数缓冲区
    output.writeObject("drawParameters.data", drawParameters->data);
    if (!drawParameters->data)
    {
        output.write("drawParameters.buffer", drawParameters->buffer);
        output.writeValue<uint32_t>("drawParameters.offset", drawParameters->offset);
        output.writeValue<uint32_t>("drawParameters.range", drawParameters->range);
    }

    // 写入绘制计数缓冲区
    output.writeObject("drawCount.data", drawCount->data);
    if (!drawCount->data)
    {
        output.write("drawCount.buffer", drawCount->buffer);
        output.writeValue<uint32_t>("drawCount.offset", drawCount->offset);
        output.writeValue<uint32_t>("drawCount.range", drawCount->range);
    }

    // 写入最大绘制次数和步长
    output.write("maxDrawCount", maxDrawCount);
    output.write("stride", stride);
}

// 编译带计数缓冲区的间接绘制命令
// 如果数据对象存在但缓冲区不存在，创建缓冲区并传输数据
// context: 编译上下文
void DrawMeshTasksIndirectCount::compile(Context& context)
{
    if ((!drawParameters->buffer && drawParameters->data) || (!drawCount->buffer && drawCount->data))
    {
        // 创建间接缓冲区并传输数据
        createBufferAndTransferData(context, {drawParameters, drawCount}, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    }
}

// 记录带计数缓冲区的间接绘制命令到命令缓冲区
// 调用Vulkan扩展函数执行带计数缓冲区的间接网格着色器绘制任务
// commandBuffer: 目标命令缓冲区
void DrawMeshTasksIndirectCount::record(vsg::CommandBuffer& commandBuffer) const
{
    Device* device = commandBuffer.getDevice();
    auto extensions = device->getExtensions();
    // 调用Vulkan扩展函数执行带计数缓冲区的间接网格着色器绘制任务
    extensions->vkCmdDrawMeshTasksIndirectCountEXT(commandBuffer, drawParameters->buffer->vk(commandBuffer.deviceID), drawParameters->offset, drawCount->buffer->vk(commandBuffer.deviceID), drawCount->offset, maxDrawCount, stride);
}
