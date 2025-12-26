/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/CopyImage.h>
#include <vsg/commands/CopyImageViewToWindow.h>
#include <vsg/commands/PipelineBarrier.h>

using namespace vsg;

// 记录复制图像视图到窗口命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 将源图像视图复制到交换链图像，包括图像布局转换和图像复制操作
// TODO: 用命令列表替换此实现，而不是当前创建命令、记录命令、删除命令的方式
void CopyImageViewToWindow::record(CommandBuffer& commandBuffer) const
{
    // TODO: 用命令列表替换此实现，而不是当前创建命令、记录命令、删除命令的方式

    // 如果图像索引无效，则不执行任何操作
    size_t imageIndex = window->imageIndex();
    if (imageIndex >= window->numFrames()) return;

    auto imageView = window->imageView(imageIndex);

    // 转换图像布局以便复制
    // 将交换链图像转换为传输目标布局
    auto imb_transitionSwapChainToWriteDest = ImageMemoryBarrier::create(
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        imageView->image,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    // 将存储图像转换为传输源布局
    auto imb_transitionStorageImageToReadSrc = vsg::ImageMemoryBarrier::create(
        0, VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        srcImageView->image,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    // 执行布局转换屏障
    auto pb_transitionStorageImageToReadSrc = PipelineBarrier::create(
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        imb_transitionSwapChainToWriteDest,
        imb_transitionStorageImageToReadSrc);

    pb_transitionStorageImageToReadSrc->record(commandBuffer);

    auto extent2D = window->extent2D();

    // 复制图像
    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.srcOffset = {0, 0, 0};
    copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.dstOffset = {0, 0, 0};
    copyRegion.extent = {extent2D.width, extent2D.height, 1};

    auto copyImage = CopyImage::create();
    copyImage->srcImage = srcImageView->image;
    copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImage->dstImage = imageView->image;
    copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyImage->regions.emplace_back(copyRegion);

    copyImage->record(commandBuffer);

    // 转换图像布局回原始状态
    // 将交换链图像转换回呈现源布局
    auto imb_transitionSwapChainToOriginal = ImageMemoryBarrier::create(
        VK_ACCESS_TRANSFER_WRITE_BIT, 0,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        imageView->image,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    // 将存储图像转换回通用布局
    auto imb_transitionStorageImageToOriginal = ImageMemoryBarrier::create(
        VK_ACCESS_TRANSFER_READ_BIT, 0,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        srcImageView->image,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    // 执行布局转换屏障
    auto pb_transitionStorageImageToOriginal = PipelineBarrier::create(
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        imb_transitionSwapChainToOriginal,
        imb_transitionStorageImageToOriginal);

    pb_transitionStorageImageToOriginal->record(commandBuffer);
}
