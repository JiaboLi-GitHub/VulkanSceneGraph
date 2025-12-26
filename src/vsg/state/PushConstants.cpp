/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/state/PushConstants.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 构造函数：创建推送常量命令（默认）
// 推送常量命令用于将常量数据推送到着色器
// 槽位2：在绑定描述符集之后执行
PushConstants::PushConstants() :
    Inherit(2) // 槽位2
{
}

// 构造函数：使用着色器阶段标志、偏移和数据创建推送常量命令
// in_stageFlags: 着色器阶段标志（指定哪些着色器阶段可以访问这些常量）
// in_offset: 推送常量缓冲区中的偏移量
// in_data: 要推送的数据对象
PushConstants::PushConstants(VkShaderStageFlags in_stageFlags, uint32_t in_offset, Data* in_data) :
    Inherit(2), // 槽位2
    stageFlags(in_stageFlags),
    offset(in_offset),
    data(in_data)
{
}

// 析构函数：销毁推送常量命令
PushConstants::~PushConstants()
{
}

// 从输入流读取推送常量命令对象
// input: 输入流对象
// 读取着色器阶段标志、偏移和数据
void PushConstants::read(Input& input)
{
    StateCommand::read(input);

    input.readValue<uint32_t>("stageFlags", stageFlags);
    input.read("offset", offset);
    input.read("data", data);
}

// 将推送常量命令对象写入输出流
// output: 输出流对象
// 写入着色器阶段标志、偏移和数据
void PushConstants::write(Output& output) const
{
    StateCommand::write(output);

    output.writeValue<uint32_t>("stageFlags", stageFlags);
    output.write("offset", offset);
    output.write("data", data);
}

// 记录推送常量命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdPushConstants命令，将常量数据推送到着色器
void PushConstants::record(CommandBuffer& commandBuffer) const
{
    vkCmdPushConstants(commandBuffer, commandBuffer.getCurrentPipelineLayout(), stageFlags, offset, static_cast<uint32_t>(data->dataSize()), data->dataPointer());
}
