/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/RenderGraph.h>
#include <vsg/app/View.h>
#include <vsg/io/Logger.h>
#include <vsg/lighting/Light.h>
#include <vsg/nodes/Bin.h>
#include <vsg/state/MultisampleState.h>
#include <vsg/vk/Context.h>
#include <vsg/vk/State.h>

using namespace vsg;

// 默认构造函数：创建空的渲染图
// 初始化视口状态和窗口调整大小处理器
RenderGraph::RenderGraph() :
    viewportState(ViewportState::create()),
    windowResizeHandler(WindowResizeHandler::create())
{
}

// 构造函数：从窗口和视图创建渲染图
// in_window: 窗口对象
// in_view: 视图对象（可选）
// 设置窗口、添加视图子节点、配置渲染区域和清除值
RenderGraph::RenderGraph(ref_ptr<Window> in_window, ref_ptr<View> in_view) :
    RenderGraph()
{
    window = in_window;

    // 如果提供了视图，将其添加为子节点
    if (in_view)
    {
        addChild(in_view);
    }

    // 记录之前的窗口尺寸
    previous_extent = window->extent2D();

    // 根据视图的相机设置渲染区域，否则使用整个窗口
    if (in_view && in_view->camera && in_view->camera->viewportState)
    {
        renderArea = in_view->camera->getRenderArea();
    }
    else
    {
        renderArea.offset = {0, 0};
        renderArea.extent = window->extent2D();
    }

    // 设置视口状态
    viewportState->set(renderArea.offset.x, renderArea.offset.y, renderArea.extent.width, renderArea.extent.height);

    // 根据渲染通道的附件设置清除值
    setClearValues(window->clearColor(), VkClearDepthStencilValue{0.0f, 0});
}

// 获取渲染通道
// 返回: 渲染通道指针，如果不存在则返回nullptr
// 按优先级返回：显式设置的渲染通道、帧缓冲区的渲染通道、窗口的渲染通道
RenderPass* RenderGraph::getRenderPass()
{
    if (renderPass)
    {
        return renderPass;
    }
    else if (framebuffer)
    {
        return framebuffer->getRenderPass();
    }
    else if (window)
    {
        return window->getOrCreateRenderPass();
    }
    return nullptr;
}

// 设置清除值
// clearColor: 颜色清除值
// clearDepthStencil: 深度模板清除值
// 根据渲染通道的附件类型设置相应的清除值（颜色附件使用颜色值，深度模板附件使用深度模板值）
void RenderGraph::setClearValues(VkClearColorValue clearColor, VkClearDepthStencilValue clearDepthStencil)
{
    auto activeRenderPass = getRenderPass();
    if (!activeRenderPass) return;

    auto& attachments = activeRenderPass->attachments;
    clearValues.resize(attachments.size());
    for (size_t i = 0; i < attachments.size(); ++i)
    {
        // 根据附件的最终布局判断是深度模板附件还是颜色附件
        if (attachments[i].finalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            clearValues[i].depthStencil = clearDepthStencil;
        }
        else
        {
            clearValues[i].color = clearColor;
        }
    }
}

// 获取渲染图的尺寸
// 返回: 2D尺寸，如果不存在则返回无效尺寸
// 按优先级返回：帧缓冲区的尺寸、窗口的尺寸、无效尺寸
VkExtent2D RenderGraph::getExtent() const
{
    if (framebuffer)
        return framebuffer->extent2D();
    else if (window)
        return window->extent2D();
    else
        return VkExtent2D{invalid_dimension, invalid_dimension};
}

// 接受记录遍历：开始渲染通道，遍历子图，结束渲染通道
// recordTraversal: 记录遍历器对象
// 检查尺寸变化，开始渲染通道，遍历子图记录命令，结束渲染通道
void RenderGraph::accept(RecordTraversal& recordTraversal) const
{
    GPU_INSTRUMENTATION_L1_NC(recordTraversal.instrumentation, *recordTraversal.getCommandBuffer(), "RenderGraph", COLOR_RECORD_L1);

    // 获取当前尺寸
    auto extent = getExtent();
    // 如果之前尺寸无效或没有窗口调整处理器，更新之前尺寸
    if (previous_extent.width == invalid_dimension || previous_extent.height == invalid_dimension || !windowResizeHandler)
    {
        previous_extent = extent;
    }
    // 如果尺寸发生变化，调用调整大小处理
    else if (previous_extent.width != extent.width || previous_extent.height != extent.height)
    {
        auto this_renderGraph = const_cast<RenderGraph*>(this);
        this_renderGraph->resized();
    }

    // 准备渲染通道开始信息
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

    // 根据是否有帧缓冲区选择渲染通道和帧缓冲区
    if (framebuffer)
    {
        renderPassInfo.renderPass = renderPass ? *(renderPass) : *(framebuffer->getRenderPass());
        renderPassInfo.framebuffer = *(framebuffer);
    }
    else
    {
        // 从窗口获取当前图像索引和对应的帧缓冲区
        size_t imageIndex = window->imageIndex();
        if (imageIndex >= window->numFrames()) return;

        renderPassInfo.renderPass = renderPass ? *(renderPass) : *(window->getRenderPass());
        renderPassInfo.framebuffer = *(window->framebuffer(imageIndex));
    }

    renderPassInfo.renderArea = renderArea;

#if 0
    info("RenderGraph() ", this, " renderArea.offset.x = ", renderArea.offset.x, ", renderArea.offset.y = ", renderArea.offset.y
                , ", renderArea.extent.width = ", renderArea.extent.width, ", renderArea.extent.height = ", renderArea.extent.height);
#endif

    // 设置清除值
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // 设置视口状态提示
    recordTraversal.getState()->viewportStateHint = viewportStateHint;

    // 开始渲染通道
    VkCommandBuffer vk_commandBuffer = *(recordTraversal.getState()->_commandBuffer);
    vkCmdBeginRenderPass(vk_commandBuffer, &renderPassInfo, contents);

    // 同步视口状态并推送
    viewportState->set(renderArea.offset.x, renderArea.offset.y, renderArea.extent.width, renderArea.extent.height);

    // 如果使用动态视口状态，推送视口状态
    if ((viewportStateHint & DYNAMIC_VIEWPORTSTATE))
    {
        recordTraversal.getState()->pushView(viewportState);

        // 遍历子图以将命令放入命令缓冲区
        traverse(recordTraversal);

        recordTraversal.getState()->popView(viewportState);
    }
    else
    {
        // 遍历子图以将命令放入命令缓冲区
        traverse(recordTraversal);
    }

    // 结束渲染通道
    vkCmdEndRenderPass(vk_commandBuffer);
}

// 处理窗口调整大小
// 当窗口或帧缓冲区尺寸发生变化时调用
// 使用窗口调整大小处理器遍历子图，缩放渲染区域和视口
void RenderGraph::resized()
{
    if (!windowResizeHandler) return;
    if (!window && !framebuffer) return;

    auto activeRenderPass = getRenderPass();
    if (!activeRenderPass) return;

    auto device = activeRenderPass->device;

    auto extent = getExtent();

    // 配置窗口调整大小处理器
    windowResizeHandler->renderArea = renderArea;
    windowResizeHandler->previous_extent = previous_extent;
    windowResizeHandler->new_extent = extent;
    windowResizeHandler->visited.clear();

    // 如果使用静态视口状态，需要重新编译管道
    if ((viewportStateHint & STATIC_VIEWPORTSTATE))
    {
        // 如果上下文不存在，创建它
        if (!windowResizeHandler->context)
        {
            ResourceRequirements resourceRequirements;
            resourceRequirements.viewportStateHint = viewportStateHint;
            windowResizeHandler->context = vsg::Context::create(device, resourceRequirements);
        }

        windowResizeHandler->context->commandPool = nullptr;
        windowResizeHandler->context->renderPass = activeRenderPass;

        // 如果使用多重采样，添加多重采样状态
        if (activeRenderPass->maxSamples != VK_SAMPLE_COUNT_1_BIT)
        {
            windowResizeHandler->context->overridePipelineStates.emplace_back(vsg::MultisampleState::create(activeRenderPass->maxSamples));
        }

        // 在重新创建任何Vulkan对象之前，确保设备空闲
        vkDeviceWaitIdle(*(device));
    }

    // 使用窗口调整大小处理器遍历子图
    traverse(*windowResizeHandler);

    // 缩放渲染区域
    windowResizeHandler->scale_rect(renderArea);

    // 更新之前尺寸
    previous_extent = extent;
}

// 为视图创建渲染图的辅助函数
// window: 窗口对象
// camera: 相机对象
// scenegraph: 场景图根节点（可选）
// contents: 子通道内容类型（主命令缓冲区或辅助命令缓冲区）
// assignHeadlight: 是否分配头灯（默认光源）
// 返回: 创建的渲染图对象
// 创建视图，可选添加头灯和场景图，然后创建并配置渲染图
ref_ptr<RenderGraph> vsg::createRenderGraphForView(ref_ptr<Window> window, ref_ptr<Camera> camera, ref_ptr<Node> scenegraph, VkSubpassContents contents, bool assignHeadlight)
{
    // 设置视图
    auto view = View::create(camera);
    // 如果请求，添加头灯（默认光源）
    if (assignHeadlight) view->addChild(createHeadlight());
    // 如果提供了场景图，将其添加为视图的子节点
    if (scenegraph) view->addChild(scenegraph);

    // 设置渲染图
    auto renderGraph = RenderGraph::create(window, view);
    renderGraph->contents = contents;

    return renderGraph;
}
