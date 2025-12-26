/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/vk/Fence.h>
#include <vsg/vk/Queue.h>

using namespace vsg;

// 构造函数：创建队列对象
// queue: Vulkan队列句柄
// queueFlags: 队列标志位（图形、计算、传输等）
// queueFamilyIndex: 队列族索引
// queueIndex: 队列索引
// 队列用于提交命令缓冲区和呈现图像
Queue::Queue(VkQueue queue, VkQueueFlags queueFlags, uint32_t queueFamilyIndex, uint32_t queueIndex) :
    _vkQueue(queue),
    _queueFlags(queueFlags),
    _queueFamilyIndex(queueFamilyIndex),
    _queueIndex(queueIndex)
{
}

// 析构函数：销毁队列对象
Queue::~Queue()
{
}

// 提交命令缓冲区（多个提交信息）
// submitInfos: 提交信息列表
// fence: 围栏对象（可选，用于同步）
// 返回: Vulkan结果
// 将多个命令缓冲区提交到队列执行
VkResult Queue::submit(const std::vector<VkSubmitInfo>& submitInfos, Fence* fence)
{
    std::scoped_lock<std::mutex> guard(_mutex);
    return vkQueueSubmit(_vkQueue, static_cast<uint32_t>(submitInfos.size()), submitInfos.data(), fence ? fence->vk() : VK_NULL_HANDLE);
}

// 提交命令缓冲区（单个提交信息）
// submitInfo: 提交信息
// fence: 围栏对象（可选，用于同步）
// 返回: Vulkan结果
// 将单个命令缓冲区提交到队列执行
VkResult Queue::submit(const VkSubmitInfo& submitInfo, Fence* fence)
{
    std::scoped_lock<std::mutex> guard(_mutex);
    return vkQueueSubmit(_vkQueue, 1, &submitInfo, fence ? fence->vk() : VK_NULL_HANDLE);
}

// 呈现图像
// info: 呈现信息
// 返回: Vulkan结果
// 将交换链图像呈现到屏幕
VkResult Queue::present(const VkPresentInfoKHR& info)
{
    std::scoped_lock<std::mutex> guard(_mutex);
    return vkQueuePresentKHR(_vkQueue, &info);
}

// 等待队列空闲
// 返回: Vulkan结果
// 等待队列中的所有操作完成
VkResult Queue::waitIdle()
{
    std::scoped_lock<std::mutex> guard(_mutex);
    return vkQueueWaitIdle(_vkQueue);
}
