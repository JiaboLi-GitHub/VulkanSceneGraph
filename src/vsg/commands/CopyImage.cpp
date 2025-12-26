/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/CopyImage.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 构造函数：创建图像复制命令（默认）
// 图像复制命令用于在相同格式的图像之间进行像素级复制
CopyImage::CopyImage()
{
}

// 记录图像复制命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdCopyImage命令，从源图像复制到目标图像（格式必须相同）
void CopyImage::record(CommandBuffer& commandBuffer) const
{
    vkCmdCopyImage(
        commandBuffer,
        srcImage->vk(commandBuffer.deviceID),
        srcImageLayout,
        dstImage->vk(commandBuffer.deviceID),
        dstImageLayout,
        static_cast<uint32_t>(regions.size()),
        regions.data());
}
