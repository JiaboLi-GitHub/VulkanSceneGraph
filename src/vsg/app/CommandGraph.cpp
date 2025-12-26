/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/CommandGraph.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/app/View.h>
#include <vsg/io/DatabasePager.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/ui/ApplicationEvent.h>
#include <vsg/utils/ShaderSet.h>
#include <vsg/vk/State.h>

using namespace vsg;

// CommandGraph类的默认构造函数
// 创建命令图对象
CommandGraph::CommandGraph()
{
    CPU_INSTRUMENTATION_L1(instrumentation);
}

// CommandGraph类的构造函数（设备和队列族）
// 使用指定的设备和队列族创建命令图
// in_device: Vulkan设备对象
// family: 队列族索引
CommandGraph::CommandGraph(ref_ptr<Device> in_device, int family) :
    device(in_device),  // Vulkan设备
    queueFamily(family),  // 队列族索引
    presentFamily(-1)  // 呈现队列族（未设置）
{
    CPU_INSTRUMENTATION_L1(instrumentation);
}

// CommandGraph类的构造函数（窗口和子节点）
// 使用指定的窗口和子节点创建命令图
// in_window: 窗口对象
// child: 子节点（可选）
CommandGraph::CommandGraph(ref_ptr<Window> in_window, ref_ptr<Node> child) :
    window(in_window),  // 窗口对象
    device(in_window->getOrCreateDevice())  // 获取或创建设备
{
    CPU_INSTRUMENTATION_L1(instrumentation);

    // 获取队列标志
    VkQueueFlags queueFlags = VK_QUEUE_GRAPHICS_BIT;
    if (window->traits()) queueFlags = window->traits()->queueFlags;

    // 获取队列族和呈现队列族
    std::tie(queueFamily, presentFamily) = device->getPhysicalDevice()->getQueueFamily(queueFlags, window->getOrCreateSurface());

    if (child) addChild(child);  // 添加子节点
}

// CommandGraph类的析构函数
CommandGraph::~CommandGraph()
{
    CPU_INSTRUMENTATION_L1(instrumentation);
}

// 获取命令缓冲区级别
// 返回主命令缓冲区级别
// 返回值：VK_COMMAND_BUFFER_LEVEL_PRIMARY
VkCommandBufferLevel CommandGraph::level() const
{
    return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
}

// 重置命令图
// 重置命令图状态（当前为空实现）
void CommandGraph::reset()
{
}

// 获取或创建记录遍历对象
// 如果记录遍历对象不存在则创建，否则预留状态空间
// 返回值：记录遍历对象
ref_ptr<RecordTraversal> CommandGraph::getOrCreateRecordTraversal()
{
    //CPU_INSTRUMENTATION_L1(instrumentation);
    if (!recordTraversal)
        recordTraversal = RecordTraversal::create(maxSlots);  // 创建新的记录遍历对象
    else
        recordTraversal->getState()->reserve(maxSlots);  // 预留状态空间

    return recordTraversal;
}

// 记录命令缓冲区
// 遍历场景图并记录命令到命令缓冲区
// recordedCommandBuffers: 已记录的命令缓冲区集合
// frameStamp: 帧戳对象
// databasePager: 数据库分页器（可选）
void CommandGraph::record(ref_ptr<RecordedCommandBuffers> recordedCommandBuffers, ref_ptr<FrameStamp> frameStamp, ref_ptr<DatabasePager> databasePager)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "CommandGraph record", COLOR_RECORD_L1);

    // 如果窗口不可见，跳过记录
    if (window && !window->visible())
    {
        //vsg::info("CommandGraph::record() ", frameStamp->frameCount, " window not visible.");
        return;
    }

    // 创建记录遍历对象（如果尚未创建）
    getOrCreateRecordTraversal();

    // 设置记录遍历对象的参数
    recordTraversal->recordedCommandBuffers = recordedCommandBuffers;
    recordTraversal->setFrameStamp(frameStamp);
    recordTraversal->setDatabasePager(databasePager);
    recordTraversal->clearBins();  // 清空分箱
    recordTraversal->regionsOfInterest.clear();  // 清空感兴趣区域

    // 查找可用的命令缓冲区（没有依赖提交的）
    ref_ptr<CommandBuffer> commandBuffer;
    for (auto& cb : _commandBuffers)
    {
        if (cb->numDependentSubmissions() == 0)
        {
            commandBuffer = cb;
            break;
        }
    }
    // 如果没有可用的，创建新的命令缓冲区
    if (!commandBuffer)
    {
        ref_ptr<CommandPool> cp = CommandPool::create(device, queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        commandBuffer = cp->allocate(level());
        _commandBuffers.push_back(commandBuffer);
    }
    else
    {
        commandBuffer->reset();  // 重置命令缓冲区
    }

    // 增加依赖提交计数
    commandBuffer->numDependentSubmissions().fetch_add(1);

    // 连接命令缓冲区到记录遍历状态
    recordTraversal->getState()->connect(commandBuffer);

    // 获取Vulkan命令缓冲区句柄
    VkCommandBuffer vk_commandBuffer = *commandBuffer;

    // 设置命令缓冲区开始信息
    // 如果嵌套在命令缓冲区内，则使用VkCommandBufferInheritanceInfo
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // 一次性提交
    beginInfo.pInheritanceInfo = nullptr;

    // 开始记录命令缓冲区
    vkBeginCommandBuffer(vk_commandBuffer, &beginInfo);

    {
        COMMAND_BUFFER_INSTRUMENTATION(instrumentation, *commandBuffer, "CommandGraph record", COLOR_RECORD)
        // 遍历场景图并记录命令
        traverse(*recordTraversal);
    }

    // 结束记录命令缓冲区
    vkEndCommandBuffer(vk_commandBuffer);

    // 将命令缓冲区添加到已记录的命令缓冲区集合
    recordedCommandBuffers->add(submitOrder, commandBuffer);
}

// 为视图创建命令图
// 创建用于渲染指定视图的命令图
// window: 窗口对象
// camera: 相机对象
// scenegraph: 场景图节点
// contents: 子通道内容
// assignHeadlight: 是否分配头灯
// 返回值：命令图对象
ref_ptr<CommandGraph> vsg::createCommandGraphForView(ref_ptr<Window> window, ref_ptr<Camera> camera, ref_ptr<Node> scenegraph, VkSubpassContents contents, bool assignHeadlight)
{
    auto commandGraph = CommandGraph::create(window);

    // 添加渲染图作为子节点
    commandGraph->addChild(createRenderGraphForView(window, camera, scenegraph, contents, assignHeadlight));

    return commandGraph;
}
