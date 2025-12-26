/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/Presentation.h>
#include <vsg/io/Logger.h>

using namespace vsg;

// 呈现图像到屏幕
// 将渲染完成的图像呈现到所有关联的窗口
// 返回值：Vulkan操作结果
VkResult Presentation::present()
{
    //debug("Presentation::present()");

    // 收集所有等待信号量
    std::vector<VkSemaphore> vk_semaphores;
    for (auto& semaphore : waitSemaphores)
    {
        vk_semaphores.emplace_back(*(semaphore));
    }

    // 收集所有交换链和图像索引
    std::vector<VkSwapchainKHR> vk_swapchains;
    std::vector<uint32_t> indices;
    for (auto& window : windows)
    {
        size_t imageIndex = window->imageIndex();  // 获取当前图像索引
        // 只处理可见窗口且索引有效的窗口
        if (window->visible() && imageIndex < window->numFrames())
        {
            vk_swapchains.emplace_back(*(window->getOrCreateSwapchain()));  // 添加交换链
            indices.emplace_back(static_cast<uint32_t>(imageIndex));  // 添加图像索引

            // 添加渲染完成信号量
            auto& renderFinishedSemaphore = window->frame(imageIndex).renderFinishedSemaphore;
            vk_semaphores.push_back(renderFinishedSemaphore->vk());
        }
    }

    // 如果没有交换链需要呈现，直接返回成功
    if (vk_swapchains.empty())
    {
        // nothing to present so return early
        return VK_SUCCESS;
    }

    // 填充呈现信息结构
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(vk_semaphores.size());  // 等待信号量数量
    presentInfo.pWaitSemaphores = vk_semaphores.data();  // 等待信号量数组
    presentInfo.swapchainCount = static_cast<uint32_t>(vk_swapchains.size());  // 交换链数量
    presentInfo.pSwapchains = vk_swapchains.data();  // 交换链数组
    presentInfo.pImageIndices = indices.data();  // 图像索引数组

#if 0
    debug( "pdo.presentInfo->present(..)");
    debug( "    presentInfo.waitSemaphoreCount = ", presentInfo.waitSemaphoreCount);
    for (uint32_t i = 0; i < presentInfo.waitSemaphoreCount; ++i)
    {
        debug( "        presentInfo.pWaitSemaphores[", i, "] = ", presentInfo.pWaitSemaphores[i]);
    }
    debug( "    presentInfo.commandBufferCount = ", presentInfo.swapchainCount);
    for (uint32_t i = 0; i < presentInfo.swapchainCount; ++i)
    {
        debug( "        presentInfo.pSwapchains[", i, "] = ", presentInfo.pSwapchains[i]);
        debug( "        presentInfo.pImageIndices[", i, "] = ", presentInfo.pImageIndices[i]);
    }
    debug("\n");
#endif

    // 调用队列的呈现方法
    return queue->present(presentInfo);
}
