/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BlitImage.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 构造函数：创建图像块传输命令（默认）
// 图像块传输命令用于在不同格式或大小的图像之间进行缩放和过滤传输
BlitImage::BlitImage()
{
}

// 记录图像块传输命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdBlitImage命令，从源图像块传输到目标图像，支持缩放和过滤
void BlitImage::record(CommandBuffer& commandBuffer) const
{
    vkCmdBlitImage(
        commandBuffer,
        srcImage->vk(commandBuffer.deviceID),
        srcImageLayout,
        dstImage->vk(commandBuffer.deviceID),
        dstImageLayout,
        static_cast<uint32_t>(regions.size()),
        regions.data(),
        filter);
}
