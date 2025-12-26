/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/CommandGraph.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/app/View.h>
#include <vsg/commands/ExecuteCommands.h>
#include <vsg/io/DatabasePager.h>
#include <vsg/lighting/Light.h>
#include <vsg/ui/ApplicationEvent.h>
#include <vsg/vk/State.h>

using namespace vsg;

// SecondaryCommandGraph类的构造函数（设备和队列族）
// 使用指定的设备和队列族创建次级命令图
// in_device: Vulkan设备对象
// family: 队列族索引
SecondaryCommandGraph::SecondaryCommandGraph(ref_ptr<Device> in_device, int family) :
    Inherit(in_device, family)  // 调用基类构造函数
{
}

// SecondaryCommandGraph类的构造函数（窗口、子节点和子通道）
// 使用指定的窗口、子节点和子通道索引创建次级命令图
// in_window: 窗口对象
// child: 子节点
// in_subpass: 子通道索引
SecondaryCommandGraph::SecondaryCommandGraph(ref_ptr<Window> in_window, ref_ptr<Node> child, uint32_t in_subpass) :
    Inherit(in_window, child),  // 调用基类构造函数
    subpass(in_subpass)  // 子通道索引
{
}

// SecondaryCommandGraph类的析构函数
SecondaryCommandGraph::~SecondaryCommandGraph()
{
}

// 获取命令缓冲区级别
// 返回次级命令缓冲区级别
// 返回值：VK_COMMAND_BUFFER_LEVEL_SECONDARY
VkCommandBufferLevel SecondaryCommandGraph::level() const
{
    return VK_COMMAND_BUFFER_LEVEL_SECONDARY;
}

// 重置次级命令图
// 重置所有关联的执行命令节点
void SecondaryCommandGraph::reset()
{
    for (auto& ec : _executeCommands) ec->reset();
}

// 连接执行命令节点
// 将执行命令节点添加到连接列表
// ec: 执行命令节点
void SecondaryCommandGraph::_connect(ExecuteCommands* ec)
{
    _executeCommands.emplace_back(ec);
}

// 断开执行命令节点
// 从连接列表中移除执行命令节点
// ec: 要断开的执行命令节点
void SecondaryCommandGraph::_disconnect(ExecuteCommands* ec)
{
    auto itr = std::find(_executeCommands.begin(), _executeCommands.end(), ec);
    if (itr != _executeCommands.end()) _executeCommands.erase(itr);
}

// 获取渲染通道
// 从渲染通道、帧缓冲区或窗口获取渲染通道
// 返回值：渲染通道对象，如果不存在则返回nullptr
RenderPass* SecondaryCommandGraph::getRenderPass()
{
    if (renderPass)
    {
        return renderPass;  // 直接返回渲染通道
    }
    else if (framebuffer)
    {
        return framebuffer->getRenderPass();  // 从帧缓冲区获取
    }
    else if (window)
    {
        return window->getOrCreateRenderPass();  // 从窗口获取或创建
    }
    return nullptr;
}

// 记录次级命令缓冲区
// 遍历场景图并记录命令到次级命令缓冲区
// recordedCommandBuffers: 已记录的命令缓冲区集合
// frameStamp: 帧戳对象
// databasePager: 数据库分页器（可选）
void SecondaryCommandGraph::record(ref_ptr<RecordedCommandBuffers> recordedCommandBuffers, ref_ptr<FrameStamp> frameStamp, ref_ptr<DatabasePager> databasePager)
{
    // 如果窗口不可见，跳过记录
    if (window && !window->visible())
    {
        return;
    }

    // 创建记录遍历对象（如果尚未创建）
    getOrCreateRecordTraversal();

    // 设置记录遍历对象的参数
    recordTraversal->recordedCommandBuffers = recordedCommandBuffers;
    recordTraversal->setFrameStamp(frameStamp);
    recordTraversal->setDatabasePager(databasePager);
    recordTraversal->clearBins();  // 清空分箱

    // 查找可用的命令缓冲区
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

    // 设置命令缓冲区到记录遍历状态
    recordTraversal->getState()->_commandBuffer = commandBuffer;

    // 获取Vulkan命令缓冲区句柄
    VkCommandBuffer vk_commandBuffer = *commandBuffer;

    // 设置命令缓冲区开始信息（次级命令缓冲区需要继承信息）
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;  // 渲染通道继续
    // 如果有多个执行命令节点，启用同时使用标志
    if (_executeCommands.size() > 1) beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    // 设置继承信息
    VkCommandBufferInheritanceInfo inheritanceInfo;
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.pNext = nullptr;
    inheritanceInfo.occlusionQueryEnable = occlusionQueryEnable;  // 遮挡查询启用
    inheritanceInfo.queryFlags = queryFlags;  // 查询标志
    inheritanceInfo.pipelineStatistics = pipelineStatistics;  // 管线统计
    beginInfo.pInheritanceInfo = &inheritanceInfo;

    // 设置渲染通道
    if (const auto activeRenderPass = getRenderPass())
        inheritanceInfo.renderPass = *(activeRenderPass);
    else
        inheritanceInfo.renderPass = VK_NULL_HANDLE;

    inheritanceInfo.subpass = subpass;  // 子通道索引

    // 设置帧缓冲区
    if (framebuffer)
    {
        inheritanceInfo.framebuffer = *framebuffer;
    }
    else if (window)
    {
        //inheritanceInfo.framebuffer = *(window->framebuffer(window->nextImageIndex()));
        inheritanceInfo.framebuffer = VK_NULL_HANDLE;
    }

    // 开始记录命令缓冲区
    vkBeginCommandBuffer(vk_commandBuffer, &beginInfo);

    // 遍历场景图并记录命令
    traverse(*recordTraversal);

    // 结束记录命令缓冲区
    vkEndCommandBuffer(vk_commandBuffer);

    // 将命令缓冲区传递给连接的ExecuteCommands节点
    for (auto& ec : _executeCommands)
    {
        ec->completed(*this, commandBuffer);
    }

    // 将命令缓冲区添加到已记录的命令缓冲区集合
    recordedCommandBuffers->add(submitOrder, commandBuffer);
}

// 为视图创建次级命令图
// 创建用于渲染指定视图的次级命令图
// window: 窗口对象
// camera: 相机对象
// scenegraph: 场景图节点
// subpass: 子通道索引
// assignHeadlight: 是否分配头灯
// 返回值：次级命令图对象
ref_ptr<SecondaryCommandGraph> vsg::createSecondaryCommandGraphForView(ref_ptr<Window> window, ref_ptr<Camera> camera, ref_ptr<Node> scenegraph, uint32_t subpass, bool assignHeadlight)
{
    // 设置视图
    auto view = View::create(camera);
    if (assignHeadlight) view->addChild(createHeadlight());  // 添加头灯
    if (scenegraph) view->addChild(scenegraph);  // 添加场景图

    auto commandGraph = SecondaryCommandGraph::create(window, view, subpass);

    return commandGraph;
}
