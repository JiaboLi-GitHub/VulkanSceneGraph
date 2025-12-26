/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/Commands.h>
#include <vsg/commands/CopyAndReleaseBuffer.h>
#include <vsg/commands/CopyAndReleaseImage.h>
#include <vsg/commands/PipelineBarrier.h>
#include <vsg/core/Version.h>
#include <vsg/io/Logger.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/state/DescriptorSet.h>
#include <vsg/state/DynamicState.h>
#include <vsg/ui/UIEvent.h>
#include <vsg/vk/CommandBuffer.h>
#include <vsg/vk/Context.h>
#include <vsg/vk/DescriptorPools.h>
#include <vsg/vk/RenderPass.h>
#include <vsg/vk/State.h>

using namespace vsg;

/////////////////////////////////////////////////////////////////////////////////////////
//
// BuildAccelerationStructureCommand - 构建加速结构命令（用于光线追踪）
//

// 构造函数：创建构建加速结构命令对象
// device: 设备对象
// info: 加速结构构建几何信息
// structure: 加速结构句柄
// primitiveCounts: 图元计数列表
// 加速结构用于光线追踪，存储场景几何信息以加速光线-几何相交测试
BuildAccelerationStructureCommand::BuildAccelerationStructureCommand(Device* device, const VkAccelerationStructureBuildGeometryInfoKHR& info, const VkAccelerationStructureKHR& structure, const std::vector<uint32_t>& primitiveCounts) :
    _device(device),
    _accelerationStructureInfo(info),
    _accelerationStructure(structure)
{
    _accelerationStructureInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    _accelerationStructureInfo.dstAccelerationStructure = _accelerationStructure;
    // 复制几何信息（因为原始指针可能失效）
    _accelerationStructureGeometries = std::vector<VkAccelerationStructureGeometryKHR>(_accelerationStructureInfo.pGeometries, _accelerationStructureInfo.pGeometries + _accelerationStructureInfo.geometryCount);
    _accelerationStructureInfo.pGeometries = _accelerationStructureGeometries.data();
    // 为每个图元计数创建构建范围信息
    for (const auto c : primitiveCounts)
    {
        _accelerationStructureBuildRangeInfos.emplace_back();
        _accelerationStructureBuildRangeInfos.back().firstVertex = 0;
        _accelerationStructureBuildRangeInfos.back().primitiveCount = c;
        _accelerationStructureBuildRangeInfos.back().primitiveOffset = 0;
        _accelerationStructureBuildRangeInfos.back().transformOffset = 0;
    }
}

// 记录命令到命令缓冲区
// commandBuffer: 命令缓冲区对象
// 将构建加速结构的命令记录到命令缓冲区，并在之后插入内存屏障
void BuildAccelerationStructureCommand::record(CommandBuffer& commandBuffer) const
{
    auto extensions = commandBuffer.getDevice()->getExtensions();
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfos = _accelerationStructureBuildRangeInfos.data();
    extensions->vkCmdBuildAccelerationStructuresKHR(
        commandBuffer,
        1,
        &_accelerationStructureInfo,
        &rangeInfos);

    // 插入内存屏障以确保加速结构构建完成
    VkMemoryBarrier memoryBarrier;
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.pNext = nullptr;
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier, 0, 0, 0, 0);
}

// 设置临时缓冲区
// scratchBuffer: 临时缓冲区对象
// 设置用于构建加速结构的临时缓冲区，并获取其设备地址
void BuildAccelerationStructureCommand::setScratchBuffer(ref_ptr<Buffer> scratchBuffer)
{
    _scratchBuffer = scratchBuffer;
    auto extensions = _device->getExtensions();
    VkBufferDeviceAddressInfo devAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, _scratchBuffer->vk(_device->deviceID)};
    _accelerationStructureInfo.scratchData.deviceAddress = extensions->vkGetBufferDeviceAddressKHR(*_device, &devAddressInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////
//
// vsg::Context - 编译上下文，管理资源池和命令缓冲区
//
// 构造函数：创建编译上下文对象
// in_device: 设备对象
// in_resourceRequirements: 资源需求
// 编译上下文用于管理场景图编译过程中的资源分配和命令记录
Context::Context(Device* in_device, const ResourceRequirements& in_resourceRequirements) :
    deviceID(in_device->deviceID),
    device(in_device),
    resourceRequirements(in_resourceRequirements),
    scratchBufferSize(0)
{
    //semaphore = vsg::Semaphore::create(device);
    scratchMemory = ScratchMemory::create(4096);

    vsg::debug("Context::Context() ", this);

    // 获取或创建设备内存缓冲区池（用于设备本地内存）
    deviceMemoryBufferPools = device->deviceMemoryBufferPools.ref_ptr();
    if (!deviceMemoryBufferPools)
    {
        device->deviceMemoryBufferPools = deviceMemoryBufferPools = MemoryBufferPools::create("Device_MemoryBufferPool", device, in_resourceRequirements);
        vsg::debug("Context::Context() creating new deviceMemoryBufferPools = ", deviceMemoryBufferPools);
    }
    else
    {
        vsg::debug("Context::Context() reusing deviceMemoryBufferPools = ", deviceMemoryBufferPools);
    }

    // 获取或创建暂存内存缓冲区池（用于主机可见内存，用于数据传输）
    stagingMemoryBufferPools = device->stagingMemoryBufferPools.ref_ptr();
    if (!stagingMemoryBufferPools)
    {
        device->stagingMemoryBufferPools = stagingMemoryBufferPools = MemoryBufferPools::create("Staging_MemoryBufferPool", device, in_resourceRequirements);
        vsg::debug("Context::Context() creating new stagingMemoryBufferPools = ", stagingMemoryBufferPools);
    }
    else
    {
        vsg::debug("Context::Context() reusing stagingMemoryBufferPools = ", stagingMemoryBufferPools);
    }

    // 获取或创建描述符池（用于描述符集分配）
    descriptorPools = device->descriptorPools.ref_ptr();
    if (!descriptorPools)
    {
        device->descriptorPools = descriptorPools = DescriptorPools::create(device);
        vsg::debug("Context::Context() creating new descriptorPools = ", descriptorPools);
    }
    else
    {
        vsg::debug("Context::Context() reusing descriptorPools = ", descriptorPools);
    }

    // 如果资源需求要求动态视口状态，添加默认的视口和裁剪动态状态
    if ((resourceRequirements.viewportStateHint & DYNAMIC_VIEWPORTSTATE))
    {
        defaultPipelineStates.push_back(DynamicState::create(VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR));
    }
}

Context::Context(const Context& context) :
    Inherit(context),
    deviceID(context.deviceID),
    device(context.device),
    view(context.view),
    viewID(context.viewID),
    mask(context.mask),
    viewDependentState(context.viewDependentState),
    renderPass(context.renderPass),
    defaultPipelineStates(context.defaultPipelineStates),
    overridePipelineStates(context.overridePipelineStates),
    descriptorPools(context.descriptorPools),
    graphicsQueue(context.graphicsQueue),
    commandPool(context.commandPool),
    deviceMemoryBufferPools(context.deviceMemoryBufferPools),
    stagingMemoryBufferPools(context.stagingMemoryBufferPools),
    scratchBufferSize(context.scratchBufferSize)
{
    scratchMemory = ScratchMemory::create(4096);
}

Context::~Context()
{
    if (requiresWaitForCompletion)
    {
        waitForCompletion();
    }
}

// 获取或创建命令缓冲区
// 返回: 命令缓冲区对象
// 如果命令缓冲区不存在，从命令池分配一个新的
ref_ptr<CommandBuffer> Context::getOrCreateCommandBuffer()
{
    if (!commandBuffer)
    {
        commandBuffer = commandPool->allocate();
    }

    return commandBuffer;
}

// 获取或创建着色器编译器
// 返回: 着色器编译器指针
// 如果着色器编译器不存在，创建一个新的并设置Vulkan版本
ShaderCompiler* Context::getOrCreateShaderCompiler()
{
    if (shaderCompiler) return shaderCompiler;

#if VSG_SUPPORTS_ShaderCompiler
    shaderCompiler = ShaderCompiler::create();

    if (device && device->getInstance())
    {
        shaderCompiler->defaults->vulkanVersion = device->getInstance()->apiVersion;
    }

#endif

    return shaderCompiler;
}

// 预留资源
// requirements: 资源需求
// 根据资源需求预留描述符池等资源
void Context::reserve(const ResourceRequirements& requirements)
{
    CPU_INSTRUMENTATION_L2_NC(instrumentation, "Context reserve", COLOR_COMPILE)

    resourceRequirements.maxSlots.merge(requirements.maxSlots);

    descriptorPools->reserve(requirements);
}

// 分配描述符集
// descriptorSetLayout: 描述符集布局
// 返回: 描述符集实现对象
// 从描述符池分配一个描述符集
ref_ptr<DescriptorSet::Implementation> Context::allocateDescriptorSet(DescriptorSetLayout* descriptorSetLayout)
{
    return descriptorPools->allocateDescriptorSet(descriptorSetLayout);
}

// 复制数据到图像
// data: 源数据对象
// dest: 目标图像信息
// 将数据复制到图像，使用暂存缓冲区进行传输
void Context::copy(ref_ptr<Data> data, ref_ptr<ImageInfo> dest)
{
    CPU_INSTRUMENTATION_L2_NC(instrumentation, "Context copy", COLOR_COMPILE)

    // info("Context::copy(", data, ", ", dest, ") ", this, ", ", transferTask);

    if (!copyImageCmd)
    {
        copyImageCmd = CopyAndReleaseImage::create(stagingMemoryBufferPools);
        commands.push_back(copyImageCmd);
    }

    copyImageCmd->copy(data, dest);
}

// 复制数据到图像（带mipmap级别）
// data: 源数据对象
// dest: 目标图像信息
// numMipMapLevels: mipmap级别数量
// 将数据复制到图像，支持多个mipmap级别
void Context::copy(ref_ptr<Data> data, ref_ptr<ImageInfo> dest, uint32_t numMipMapLevels)
{
    CPU_INSTRUMENTATION_L2_NC(instrumentation, "Context copy", COLOR_COMPILE)

    // info("Context::copy(", data, ", ", dest, ") ", this, ", ", transferTask);

    if (!copyImageCmd)
    {
        copyImageCmd = CopyAndReleaseImage::create(stagingMemoryBufferPools);
        commands.push_back(copyImageCmd);
    }

    copyImageCmd->copy(data, dest, numMipMapLevels);
}

// 复制缓冲区
// src: 源缓冲区信息
// dest: 目标缓冲区信息
// 将数据从一个缓冲区复制到另一个缓冲区
void Context::copy(ref_ptr<BufferInfo> src, ref_ptr<BufferInfo> dest)
{
    CPU_INSTRUMENTATION_L2_NC(instrumentation, "Context copy", COLOR_COMPILE)

    // info("Context::copy(", src, ", ", dest, ") ", this, ", ", transferTask);

    if (!copyBufferCmd)
    {
        copyBufferCmd = CopyAndReleaseBuffer::create();
        commands.emplace_back(copyBufferCmd);
    }

    copyBufferCmd->add(src, dest);
}

// 记录命令到命令缓冲区并提交
// 返回: 如果有命令记录则返回true
// 将所有待处理的命令记录到命令缓冲区，然后提交到图形队列执行
bool Context::record()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Context record", COLOR_COMPILE)

    if (commands.empty() && buildAccelerationStructureCommands.empty()) return false;

    // 获取或创建围栏（用于同步）
    if (!fence)
    {
        fence = vsg::Fence::create(device);
    }
    else
    {
        fence->reset();
    }

    getOrCreateCommandBuffer();

    // 开始记录命令缓冲区
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(*commandBuffer, &beginInfo);

    {
        COMMAND_BUFFER_INSTRUMENTATION(instrumentation, *commandBuffer, "Context record", COLOR_COMPILE)

        // 记录所有命令
        {
            for (auto& command : commands) command->record(*commandBuffer);
        }

        // 创建临时缓冲区并记录构建加速结构命令
        if (scratchBufferSize > 0)
        {
            VkMemoryAllocateFlagsInfo memFlags = {};
            memFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            memFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            ref_ptr<Buffer> scratchBuffer = vsg::createBufferAndMemory(device, scratchBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memFlags);

            for (auto& command : buildAccelerationStructureCommands)
            {
                command->setScratchBuffer(scratchBuffer);
                command->record(*commandBuffer);
            }
        }
    }

    vkEndCommandBuffer(*commandBuffer);

    // 准备提交信息
    VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = commandBuffer->data();
    if (semaphore)
    {
        vsg::info("Context::record() semaphore assigned to submitInfo ", semaphore);

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = semaphore->data();
        submitInfo.pWaitDstStageMask = &waitDstStageMask;
    }
    else
    {
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;
    }

    // 提交到图形队列
    graphicsQueue->submit(submitInfo, fence);

    requiresWaitForCompletion = true;

    return true;
}

// 等待命令执行完成
// 等待所有提交的命令执行完成，然后清理命令列表
void Context::waitForCompletion()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Context waitForCompletion", COLOR_COMPILE)

    if (!requiresWaitForCompletion || !commandBuffer || !fence)
    {
        return;
    }

    // auto start_point = vsg::clock::now();

    // 必须等待队列清空后才能安全清理命令缓冲区
    uint64_t timeout = 1000000000;

    VkResult result;
    while ((result = fence->wait(timeout)) == VK_TIMEOUT)
    {
        info("Context::waitForCompletion() ", this, " fence->wait() timed out, trying again.");
    }

    if (result != VK_SUCCESS)
    {
        info("Context::waitForCompletion()  ", this, " fence->wait() failed with error. VkResult = ", result);
    }

    //vsg::info("Context::waitForCompletion() ", std::chrono::duration<double, std::chrono::milliseconds::period>(vsg::clock::now() - start_point).count());

    requiresWaitForCompletion = false;
    commands.clear();
    copyImageCmd = nullptr;
    copyBufferCmd = nullptr;
}
