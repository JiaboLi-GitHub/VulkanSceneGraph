/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/NextSubPass.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 析构函数：销毁下一个子通道命令
// 下一个子通道命令用于在渲染通道中移动到下一个子通道
NextSubPass::~NextSubPass()
{
}

// 从输入流读取下一个子通道命令对象
// input: 输入流对象
// 读取内容标志（指定辅助命令缓冲区的内容类型）
void NextSubPass::read(Input& input)
{
    Command::read(input);

    input.readValue<uint32_t>("contents", contents);
}

// 将下一个子通道命令对象写入输出流
// output: 输出流对象
// 写入内容标志
void NextSubPass::write(Output& output) const
{
    Command::write(output);

    output.writeValue<uint32_t>("contents", contents);
}

// 记录下一个子通道命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdNextSubpass命令，移动到渲染通道的下一个子通道
void NextSubPass::record(CommandBuffer& commandBuffer) const
{
    vkCmdNextSubpass(commandBuffer, contents);
}
