/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/ClearAttachments.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

// 构造函数：创建清除附件命令（默认）
// 清除附件命令用于清除渲染附件（颜色、深度、模板）的指定区域
ClearAttachments::ClearAttachments()
{
}

// 构造函数：使用附件列表和矩形列表创建清除附件命令
// in_attachments: 附件列表（包含清除的方面掩码、颜色附件索引和清除值）
// in_rects: 矩形列表（指定要清除的区域，包括偏移、范围、基础数组层和层数）
ClearAttachments::ClearAttachments(const Attachments& in_attachments, const Rects& in_rects) :
    attachments(in_attachments),
    rects(in_rects)
{
}

// 从输入流读取清除附件命令对象
// input: 输入流对象
// 读取附件列表（方面掩码、颜色附件索引、清除值）和矩形列表（矩形、基础数组层、层数）
void ClearAttachments::read(Input& input)
{
    Command::read(input);

    attachments.resize(input.readValue<uint32_t>("attachments"));
    for (auto& attachment : attachments)
    {
        input.readValue<uint32_t>("aspectMask", attachment.aspectMask);
        input.read("colorAttachment", attachment.colorAttachment);
        input.read("clearValue", attachment.clearValue);
    }

    rects.resize(input.readValue<uint32_t>("rects"));
    for (auto& r : rects)
    {
        input.read("rect", r.rect.offset.x, r.rect.offset.y, r.rect.extent.width, r.rect.extent.height);
        input.read("baseArrayLayer", r.baseArrayLayer);
        input.read("layerCount", r.layerCount);
    }
}

// 将清除附件命令对象写入输出流
// output: 输出流对象
// 写入附件列表和矩形列表
void ClearAttachments::write(Output& output) const
{
    Command::write(output);

    output.writeValue<uint32_t>("attachments", attachments.size());
    for (auto& attachment : attachments)
    {
        output.writeValue<uint32_t>("aspectMask", attachment.aspectMask);
        output.write("colorAttachment", attachment.colorAttachment);
        output.write("clearValue", attachment.clearValue);
    }

    output.writeValue<uint32_t>("rects", rects.size());
    for (auto& r : rects)
    {
        output.write("rect", r.rect.offset.x, r.rect.offset.y, r.rect.extent.width, r.rect.extent.height);
        output.write("baseArrayLayer", r.baseArrayLayer);
        output.write("layerCount", r.layerCount);
    }
}

// 记录清除附件命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 执行vkCmdClearAttachments命令，清除指定的附件区域
void ClearAttachments::record(CommandBuffer& commandBuffer) const
{
    vkCmdClearAttachments(commandBuffer,
                          static_cast<uint32_t>(attachments.size()), attachments.data(),
                          static_cast<uint32_t>(rects.size()), rects.data());
}
