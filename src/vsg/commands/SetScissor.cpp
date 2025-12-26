/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/SetScissor.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 构造函数：创建设置裁剪矩形命令（默认）
// 初始化首裁剪矩形索引为0
SetScissor::SetScissor() :
    firstScissor(0)
{
}

// 构造函数：使用首裁剪矩形索引和裁剪矩形列表创建设置裁剪矩形命令
// in_firstScissor: 首裁剪矩形索引（从哪个裁剪矩形开始设置）
// in_scissors: 裁剪矩形列表（包含offset和extent）
SetScissor::SetScissor(uint32_t in_firstScissor, const Scissors& in_scissors) :
    firstScissor(in_firstScissor),
    scissors(in_scissors)
{
}

// 记录设置裁剪矩形命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdSetScissor命令，设置裁剪矩形状态
void SetScissor::record(CommandBuffer& commandBuffer) const
{
    vkCmdSetScissor(commandBuffer, firstScissor, static_cast<uint32_t>(scissors.size()), scissors.data());
}
