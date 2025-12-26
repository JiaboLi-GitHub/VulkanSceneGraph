/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/meshshaders/DrawMeshTasksIndirect.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// DrawMeshTasksIndirect类的默认构造函数
// 创建间接网格着色器绘制任务命令，从缓冲区读取绘制参数
DrawMeshTasksIndirect::DrawMeshTasksIndirect() :
    drawParameters(BufferInfo::create())
{
}

// DrawMeshTasksIndirect类的构造函数
// 从数据对象创建间接绘制命令
// data: 包含绘制参数的数据对象
// in_drawCount: 绘制次数
// in_stride: 绘制参数之间的步长（字节）
DrawMeshTasksIndirect::DrawMeshTasksIndirect(ref_ptr<Data> data, uint32_t in_drawCount, uint32_t in_stride) :
    drawParameters(BufferInfo::create(data)),
    drawCount(in_drawCount),
    stride(in_stride)
{
}

// DrawMeshTasksIndirect类的构造函数
// 从缓冲区创建间接绘制命令
// in_buffer: 包含绘制参数的缓冲区
// in_offset: 缓冲区中的偏移量
// in_drawCount: 绘制次数
// in_stride: 绘制参数之间的步长（字节）
DrawMeshTasksIndirect::DrawMeshTasksIndirect(ref_ptr<Buffer> in_buffer, VkDeviceSize in_offset, uint32_t in_drawCount, uint32_t in_stride) :
    drawParameters(BufferInfo::create(in_buffer, in_offset, in_drawCount * in_stride)),
    drawCount(in_drawCount),
    stride(in_stride)
{
}

// 从输入流读取DrawMeshTasksIndirect对象
// 读取绘制参数缓冲区信息和绘制参数
void DrawMeshTasksIndirect::read(Input& input)
{
    // 尝试读取数据对象
    input.readObject("drawParameters.data", drawParameters->data);
    if (!drawParameters->data)
    {
        // 如果没有数据对象，读取缓冲区信息
        input.read("drawParameters.buffer", drawParameters->buffer);
        input.readValue<uint32_t>("drawParameters.offset", drawParameters->offset);
        input.readValue<uint32_t>("drawParameters.range", drawParameters->range);
    }

    // 读取绘制次数和步长
    input.read("drawCount", drawCount);
    input.read("stride", stride);
}

// 将DrawMeshTasksIndirect对象写入输出流
// 写入绘制参数缓冲区信息和绘制参数
void DrawMeshTasksIndirect::write(Output& output) const
{
    // 写入数据对象
    output.writeObject("drawParameters.data", drawParameters->data);
    if (!drawParameters->data)
    {
        // 如果没有数据对象，写入缓冲区信息
        output.write("drawParameters.buffer", drawParameters->buffer);
        output.writeValue<uint32_t>("drawParameters.offset", drawParameters->offset);
        output.writeValue<uint32_t>("drawParameters.range", drawParameters->range);
    }

    // 写入绘制次数和步长
    output.write("drawCount", drawCount);
    output.write("stride", stride);
}

// 编译间接绘制命令
// 如果数据对象存在但缓冲区不存在，创建缓冲区并传输数据
// context: 编译上下文
void DrawMeshTasksIndirect::compile(Context& context)
{
    if (!drawParameters->buffer && drawParameters->data)
    {
        // 创建间接缓冲区并传输数据
        createBufferAndTransferData(context, {drawParameters}, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    }
}

// 记录间接绘制命令到命令缓冲区
// 调用Vulkan扩展函数执行间接网格着色器绘制任务
// commandBuffer: 目标命令缓冲区
void DrawMeshTasksIndirect::record(vsg::CommandBuffer& commandBuffer) const
{
    Device* device = commandBuffer.getDevice();
    auto extensions = device->getExtensions();
    // 调用Vulkan扩展函数执行间接网格着色器绘制任务
    extensions->vkCmdDrawMeshTasksIndirectEXT(commandBuffer, drawParameters->buffer->vk(commandBuffer.deviceID), drawParameters->offset, drawCount, stride);
}
