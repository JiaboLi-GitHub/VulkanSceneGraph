/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/DrawIndexedIndirect.h>

using namespace vsg;

// 构造函数：创建间接索引绘制命令（默认）
// 间接索引绘制命令用于从缓冲区读取索引绘制参数，支持GPU驱动的渲染
DrawIndexedIndirect::DrawIndexedIndirect()
{
}

// 构造函数：使用数据对象创建间接索引绘制命令
// data: 包含间接索引绘制参数的数据对象
// in_drawCount: 绘制调用次数
// in_stride: 每个绘制参数之间的字节步长
DrawIndexedIndirect::DrawIndexedIndirect(ref_ptr<Data> data, uint32_t in_drawCount, uint32_t in_stride) :
    bufferInfo(BufferInfo::create(data)),
    drawCount(in_drawCount),
    stride(in_stride)
{
}

// 构造函数：使用缓冲区和偏移创建间接索引绘制命令
// in_buffer: 包含间接索引绘制参数的缓冲区
// in_offset: 缓冲区中的偏移量
// in_drawCount: 绘制调用次数
// in_stride: 每个绘制参数之间的字节步长
DrawIndexedIndirect::DrawIndexedIndirect(ref_ptr<Buffer> in_buffer, VkDeviceSize in_offset, uint32_t in_drawCount, uint32_t in_stride) :
    bufferInfo(BufferInfo::create(in_buffer, in_offset, in_drawCount * in_stride)),
    drawCount(in_drawCount),
    stride(in_stride)
{
}

// 从输入流读取间接索引绘制命令对象
// input: 输入流对象
// 读取数据对象或缓冲区信息，以及绘制次数和步长
void DrawIndexedIndirect::read(Input& input)
{
    Command::read(input);

    ref_ptr<Data> data;
    input.read("data", data);
    if (data)
    {
        bufferInfo = BufferInfo::create(data);
    }
    else
    {
        ref_ptr<Buffer> buffer;
        VkDeviceSize offset = 0;
        VkDeviceSize range = 0;
        input.readObject("buffer", bufferInfo->buffer);
        input.readValue<uint32_t>("offset", offset);
        input.readValue<uint32_t>("range", range);
        if (buffer)
        {
            bufferInfo = BufferInfo::create(buffer, offset, range);
        }
    }

    input.read("drawCount", drawCount);
    input.read("stride", stride);
}

// 将间接索引绘制命令对象写入输出流
// output: 输出流对象
// 写入数据对象或缓冲区信息，以及绘制次数和步长
void DrawIndexedIndirect::write(Output& output) const
{
    Command::write(output);

    if (bufferInfo)
    {
        output.writeObject("data", bufferInfo->data);
        if (!bufferInfo->data)
        {
            output.writeObject("buffer", bufferInfo->buffer);
            output.writeValue<uint32_t>("offset", bufferInfo->offset);
            output.writeValue<uint32_t>("range", bufferInfo->range);
        }
    }
    else
    {
        output.writeObject("data", nullptr);
        output.writeObject("buffer", nullptr);
        output.writeValue<uint32_t>("offset", 0);
        output.writeValue<uint32_t>("range", 0);
    }

    output.write("drawCount", drawCount);
    output.write("stride", stride);
}

// 编译间接索引绘制命令
// context: 编译上下文对象
// 如果缓冲区信息包含数据但还没有缓冲区，则创建缓冲区并传输数据
void DrawIndexedIndirect::compile(Context& context)
{
    if (!bufferInfo->buffer && bufferInfo->data)
    {
        createBufferAndTransferData(context, {bufferInfo}, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    }
}

// 记录间接索引绘制命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdDrawIndexedIndirect命令，从缓冲区读取索引绘制参数并执行绘制
void DrawIndexedIndirect::record(CommandBuffer& commandBuffer) const
{
    vkCmdDrawIndexedIndirect(commandBuffer, bufferInfo->buffer->vk(commandBuffer.deviceID), bufferInfo->offset, drawCount, stride);
}
