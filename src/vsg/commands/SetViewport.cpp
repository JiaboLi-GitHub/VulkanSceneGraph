/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/SetViewport.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 构造函数：创建设置视口命令（默认）
// 初始化首视口索引为0
SetViewport::SetViewport() :
    firstViewport(0)
{
}

// 构造函数：使用首视口索引和视口列表创建设置视口命令
// in_firstViewport: 首视口索引（从哪个视口开始设置）
// in_viewports: 视口列表（包含x、y、width、height、minDepth、maxDepth）
SetViewport::SetViewport(uint32_t in_firstViewport, const Viewports& in_viewports) :
    firstViewport(in_firstViewport),
    viewports(in_viewports)
{
}

// 记录设置视口命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdSetViewport命令，设置视口状态
void SetViewport::record(CommandBuffer& commandBuffer) const
{
    vkCmdSetViewport(commandBuffer, firstViewport, static_cast<uint32_t>(viewports.size()), viewports.data());
}
