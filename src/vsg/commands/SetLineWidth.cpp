/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/SetLineWidth.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 构造函数：使用线宽创建设置线宽命令
// in_lineWidth: 线宽值（以像素为单位）
// 设置线宽命令用于设置线框模式下的线宽
SetLineWidth::SetLineWidth(float in_lineWidth) :
    lineWidth(in_lineWidth)
{
}

// 记录设置线宽命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdSetLineWidth命令，设置线宽状态
void SetLineWidth::record(CommandBuffer& commandBuffer) const
{
    vkCmdSetLineWidth(commandBuffer, lineWidth);
}
