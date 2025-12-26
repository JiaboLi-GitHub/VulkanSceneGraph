/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/vk/Semaphore.h>

using namespace vsg;

// 构造函数：创建信号量对象
// device: 设备对象
// pipelineStageFlags: 管道阶段标志位（用于同步）
// pNextCreateInfo: 扩展创建信息（可选）
// 信号量用于同步队列操作，例如等待图像可用或命令缓冲区完成
Semaphore::Semaphore(Device* device, VkPipelineStageFlags pipelineStageFlags, void* pNextCreateInfo) :
    _pipelineStageFlags(pipelineStageFlags),
    _device(device)
{
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = pNextCreateInfo;

    VkResult result = vkCreateSemaphore(*device, &semaphoreInfo, _device->getAllocationCallbacks(), &_semaphore);
    if (result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create semaphore.", result};
    }
}

// 析构函数：销毁信号量对象
// 销毁Vulkan信号量
Semaphore::~Semaphore()
{
    if (_semaphore)
    {
        vkDestroySemaphore(*_device, _semaphore, _device->getAllocationCallbacks());
    }
}
