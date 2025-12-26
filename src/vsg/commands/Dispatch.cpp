/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/Dispatch.h>

using namespace vsg;

// 从输入流读取调度命令对象
// input: 输入流对象
// 读取工作组数量（X、Y、Z三个维度）
// 调度命令用于执行计算着色器
void Dispatch::read(Input& input)
{
    Command::read(input);

    input.read("groupCountX", groupCountX);
    input.read("groupCountY", groupCountY);
    input.read("groupCountZ", groupCountZ);
}

// 将调度命令对象写入输出流
// output: 输出流对象
// 写入工作组数量（X、Y、Z三个维度）
void Dispatch::write(Output& output) const
{
    Command::write(output);

    output.write("groupCountX", groupCountX);
    output.write("groupCountY", groupCountY);
    output.write("groupCountZ", groupCountZ);
}

// 记录调度命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdDispatch命令，调度计算着色器工作组
void Dispatch::record(CommandBuffer& commandBuffer) const
{
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}
