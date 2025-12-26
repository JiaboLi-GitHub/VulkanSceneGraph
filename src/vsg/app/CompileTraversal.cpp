/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/CompileTraversal.h>

#include <vsg/app/CommandGraph.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/app/SecondaryCommandGraph.h>
#include <vsg/app/View.h>
#include <vsg/app/Viewer.h>
#include <vsg/commands/Command.h>
#include <vsg/commands/Commands.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/state/MultisampleState.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/utils/ShaderSet.h>
#include <vsg/vk/RenderPass.h>
#include <vsg/vk/State.h>

using namespace vsg;

// 拷贝构造函数：从另一个编译遍历器创建新的编译遍历器
// ct: 要拷贝的编译遍历器
// 为每个上下文创建新的上下文副本
CompileTraversal::CompileTraversal(const CompileTraversal& ct) :
    Inherit(ct)
{
    for (auto& context : ct.contexts)
    {
        contexts.push_back(Context::create(*context));
    }
}

// 构造函数：从设备创建编译遍历器
// device: Vulkan设备对象
// resourceRequirements: 资源需求配置
// 设置覆盖掩码为全部，并添加设备上下文
CompileTraversal::CompileTraversal(ref_ptr<Device> device, const ResourceRequirements& resourceRequirements)
{
    overrideMask = vsg::MASK_ALL;
    add(device, resourceRequirements);
}

// 构造函数：从窗口和视口创建编译遍历器
// window: 窗口对象
// viewport: 视口状态
// resourceRequirements: 资源需求配置
// 设置覆盖掩码为全部，并添加窗口和视口上下文
CompileTraversal::CompileTraversal(Window& window, ref_ptr<ViewportState> viewport, const ResourceRequirements& resourceRequirements)
{
    overrideMask = vsg::MASK_ALL;
    add(window, viewport, resourceRequirements);
}

// 构造函数：从查看器创建编译遍历器
// viewer: 查看器对象
// resourceRequirements: 资源需求配置
// 设置覆盖掩码为全部，并添加查看器的所有窗口和视图上下文
CompileTraversal::CompileTraversal(const Viewer& viewer, const ResourceRequirements& resourceRequirements)
{
    overrideMask = vsg::MASK_ALL;
    add(viewer, resourceRequirements);
}

CompileTraversal::~CompileTraversal()
{
}

// 添加设备和传输任务到编译遍历器
// device: Vulkan设备对象
// transferTask: 传输任务对象（可选）
// resourceRequirements: 资源需求配置
// 创建编译上下文，配置命令池、图形队列和传输任务
void CompileTraversal::add(ref_ptr<Device> device, ref_ptr<TransferTask> transferTask, const ResourceRequirements& resourceRequirements)
{
    // 获取图形队列族
    auto queueFamily = device->getPhysicalDevice()->getQueueFamily(queueFlags);
    // 创建编译上下文
    auto context = Context::create(device, resourceRequirements);
    context->instrumentation = instrumentation;
    // 创建命令池（支持重置命令缓冲区）
    context->commandPool = CommandPool::create(device, queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    // 获取图形队列
    context->graphicsQueue = device->getQueue(queueFamily, queueFamilyIndex);
    // 设置传输任务
    context->transferTask = transferTask;
    contexts.push_back(context);
}

void CompileTraversal::add(ref_ptr<Device> device, const ResourceRequirements& resourceRequirements)
{
    add(device, nullptr, resourceRequirements);
}

void CompileTraversal::add(Window& window, ref_ptr<TransferTask> transferTask, ref_ptr<ViewportState> viewport, const ResourceRequirements& resourceRequirements)
{
    auto device = window.getOrCreateDevice();
    auto renderPass = window.getOrCreateRenderPass();
    auto queueFamily = device->getPhysicalDevice()->getQueueFamily(queueFlags);
    auto context = Context::create(device, resourceRequirements);
    context->instrumentation = instrumentation;
    context->renderPass = renderPass;
    context->commandPool = CommandPool::create(device, queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    context->graphicsQueue = device->getQueue(queueFamily, queueFamilyIndex);
    context->transferTask = transferTask;

    if (viewport)
        context->defaultPipelineStates.emplace_back(viewport);
    else
        context->defaultPipelineStates.emplace_back(vsg::ViewportState::create(window.extent2D()));

    if (renderPass->maxSamples != VK_SAMPLE_COUNT_1_BIT) context->overridePipelineStates.emplace_back(MultisampleState::create(renderPass->maxSamples));

    contexts.push_back(context);
}

void CompileTraversal::add(Window& window, ref_ptr<ViewportState> viewport, const ResourceRequirements& resourceRequirements)
{
    add(window, nullptr, viewport, resourceRequirements);
}

void CompileTraversal::add(Window& window, ref_ptr<TransferTask> transferTask, ref_ptr<View> view, const ResourceRequirements& resourceRequirements)
{
    auto device = window.getOrCreateDevice();
    auto renderPass = window.getOrCreateRenderPass();
    auto queueFamily = device->getPhysicalDevice()->getQueueFamily(queueFlags);
    auto context = Context::create(device, resourceRequirements);
    context->instrumentation = instrumentation;
    context->renderPass = renderPass;
    context->commandPool = vsg::CommandPool::create(device, queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    context->graphicsQueue = device->getQueue(queueFamily, queueFamilyIndex);
    context->transferTask = transferTask;

    if (renderPass->maxSamples != VK_SAMPLE_COUNT_1_BIT) context->overridePipelineStates.emplace_back(vsg::MultisampleState::create(renderPass->maxSamples));

    context->overridePipelineStates.insert(context->overridePipelineStates.end(), view->overridePipelineStates.begin(), view->overridePipelineStates.end());

    if (view->camera && view->camera->viewportState)
        context->defaultPipelineStates.emplace_back(view->camera->viewportState);
    else
        context->defaultPipelineStates.emplace_back(vsg::ViewportState::create(window.extent2D()));

    context->view = view.get();
    context->viewID = view->viewID;
    context->viewDependentState = view->viewDependentState;

    contexts.push_back(context);

    if (view->viewDependentState) addViewDependentState(*(view->viewDependentState), device, transferTask, resourceRequirements);
}

void CompileTraversal::add(Window& window, ref_ptr<View> view, const ResourceRequirements& resourceRequirements)
{
    add(window, nullptr, view, resourceRequirements);
}

void CompileTraversal::add(Framebuffer& framebuffer, ref_ptr<TransferTask> transferTask, ref_ptr<View> view, const ResourceRequirements& resourceRequirements)
{
    ref_ptr<Device> device(framebuffer.getDevice());
    auto context = Context::create(device, resourceRequirements);
    auto queueFamily = device->getPhysicalDevice()->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    context->instrumentation = instrumentation;
    context->commandPool = vsg::CommandPool::create(device, queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    context->graphicsQueue = device->getQueue(queueFamily, queueFamilyIndex);
    context->transferTask = transferTask;

    add(context, framebuffer, transferTask, view, resourceRequirements);
}

void CompileTraversal::add(ref_ptr<Context> context, Framebuffer& framebuffer, ref_ptr<TransferTask> transferTask, ref_ptr<View> view, const ResourceRequirements& resourceRequirements)
{
    ref_ptr<Device> device(framebuffer.getDevice());
    auto renderPass = context->renderPass = framebuffer.getRenderPass();
    if (renderPass->maxSamples != VK_SAMPLE_COUNT_1_BIT) context->overridePipelineStates.emplace_back(vsg::MultisampleState::create(renderPass->maxSamples));
    context->overridePipelineStates.insert(context->overridePipelineStates.end(), view->overridePipelineStates.begin(), view->overridePipelineStates.end());

    if (view->camera && view->camera->viewportState)
        context->defaultPipelineStates.emplace_back(view->camera->viewportState);
    else
        context->defaultPipelineStates.emplace_back(vsg::ViewportState::create(framebuffer.extent2D()));

    context->view = view.get();
    context->viewID = view->viewID;
    context->viewDependentState = view->viewDependentState;

    contexts.push_back(context);

    if (view->viewDependentState) addViewDependentState(*(view->viewDependentState), device, transferTask, resourceRequirements);
}

void CompileTraversal::add(Framebuffer& framebuffer, ref_ptr<View> view, const ResourceRequirements& resourceRequirements)
{
    add(framebuffer, nullptr, view, resourceRequirements);
}

void CompileTraversal::add(const Viewer& viewer, const ResourceRequirements& resourceRequirements)
{
    if (viewer.instrumentation) instrumentation = viewer.instrumentation;

    struct AddViews : public Visitor
    {
        CompileTraversal* ct = nullptr;
        const ResourceRequirements& resourceRequirements;
        ref_ptr<TransferTask> transferTask;

        AddViews(CompileTraversal* in_ct, const ResourceRequirements& in_rr) :
            ct(in_ct), resourceRequirements(in_rr){};

        const char* className() const noexcept override { return "vsg::CompileTraversal::AddViews"; }

        std::stack<ref_ptr<Object>> objectStack;

        void apply(Object& object) override
        {
            object.traverse(*this);
        }

        void apply(RenderGraph& rg) override
        {
            if (rg.window)
                objectStack.emplace(rg.window);
            else
                objectStack.emplace(rg.framebuffer);

            rg.traverse(*this);

            objectStack.pop();
        }

        void apply(View& view) override
        {
            if (!objectStack.empty())
            {
                auto obj = objectStack.top();
                if (auto window = obj.cast<Window>())
                    ct->add(*window, transferTask, ref_ptr<View>(&view), resourceRequirements);
                else if (auto framebuffer = obj.cast<Framebuffer>())
                    ct->add(*framebuffer, transferTask, ref_ptr<View>(&view), resourceRequirements);
            }
        }
    } addViews(this, resourceRequirements);

    for (const auto& task : viewer.recordAndSubmitTasks)
    {
        if (resourceRequirements.dataTransferHint == COMPILE_TRAVERSAL_USE_TRANSFER_TASK)
        {
            addViews.transferTask = task->transferTask;
        }

        for (auto& cg : task->commandGraphs)
        {
            cg->accept(addViews);
        }
    }
}

// 添加视图依赖状态（如阴影贴图）到编译遍历器
// viewDependentState: 视图依赖状态对象
// device: Vulkan设备对象
// transferTask: 传输任务对象（可选）
// resourceRequirements: 资源需求配置
// 如果视图依赖状态包含阴影贴图，则为其创建编译上下文并编译
void CompileTraversal::addViewDependentState(ViewDependentState& viewDependentState, ref_ptr<Device> device, ref_ptr<TransferTask> transferTask, const ResourceRequirements& resourceRequirements)
{
    if (viewDependentState.shadowMaps.size() > 0)
    {
        // 为阴影贴图创建编译上下文
        auto context = Context::create(device, resourceRequirements);
        auto queueFamily = device->getPhysicalDevice()->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        context->instrumentation = instrumentation;
        context->commandPool = vsg::CommandPool::create(device, queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        context->graphicsQueue = device->getQueue(queueFamily, queueFamilyIndex);
        context->transferTask = transferTask;

        // 获取第一个阴影贴图
        auto& shadowMap = viewDependentState.shadowMaps.front();
        auto& nested_framebuffer = shadowMap.renderGraph->framebuffer;
        // 如果帧缓冲区未创建，先编译视图依赖状态
        if (!nested_framebuffer)
        {
            viewDependentState.compile(*context);
            context->record();
            context->waitForCompletion();
        }

        // 如果帧缓冲区已创建，添加阴影贴图的帧缓冲区和视图
        if (nested_framebuffer)
        {
            add(context, *nested_framebuffer, transferTask, shadowMap.view, resourceRequirements);
        }
        else
        {
            vsg::info("CompileTraversal::addViewDependentState(.., ", device, ", ", transferTask, "..) no framebuffer.");
        }
    }
}

// 为编译遍历器分配性能分析工具
// in_instrumentation: 性能分析工具对象
// 为遍历器和所有上下文分配性能分析工具（为线程安全可能需要复制）
void CompileTraversal::assignInstrumentation(ref_ptr<Instrumentation> in_instrumentation)
{
    instrumentation = in_instrumentation;
    for (const auto& context : contexts)
    {
        // 为线程安全共享或复制性能分析工具
        context->instrumentation = shareOrDuplicateForThreadSafety(instrumentation);
    }
}

// 访问对象：遍历对象的所有子节点
// object: 要编译的对象
void CompileTraversal::apply(Object& object)
{
    CPU_INSTRUMENTATION_L2_NC(instrumentation, "CompileTraversal Object", COLOR_COMPILE);

    object.traverse(*this);
}

// 访问可编译节点：为所有上下文编译节点，然后遍历子节点
// node: 要编译的节点
// 为每个上下文调用节点的编译方法，然后继续遍历子节点
void CompileTraversal::apply(Compilable& node)
{
    CPU_INSTRUMENTATION_L3_NC(instrumentation, "CompileTraversal Compilable", COLOR_COMPILE);

    // 为所有上下文编译节点
    for (auto& context : contexts)
    {
        node.compile(*context);
    }

    // 继续遍历子节点
    node.traverse(*this);
}

// 访问命令集合：为所有上下文编译命令集合
// commands: 要编译的命令集合
// 为每个上下文调用命令集合的编译方法
void CompileTraversal::apply(Commands& commands)
{
    CPU_INSTRUMENTATION_L3_NC(instrumentation, "CompileTraversal Commands", COLOR_COMPILE);

    for (auto& context : contexts)
    {
        commands.compile(*context);
    }
}

// 访问几何体：为所有上下文编译几何体，然后遍历子节点
// geometry: 要编译的几何体
// 为每个上下文调用几何体的编译方法，然后继续遍历子节点
void CompileTraversal::apply(Geometry& geometry)
{
    CPU_INSTRUMENTATION_L3_NC(instrumentation, "CompileTraversal Geometry", COLOR_COMPILE);

    // 为所有上下文编译几何体
    for (auto& context : contexts)
    {
        geometry.compile(*context);
    }
    // 继续遍历子节点
    geometry.traverse(*this);
}

// 访问命令图：合并最大槽位，然后遍历子节点
// commandGraph: 要编译的命令图
// 合并所有上下文的最大槽位到命令图，然后继续遍历子节点
void CompileTraversal::apply(CommandGraph& commandGraph)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "CompileTraversal CommandGraph", COLOR_COMPILE);

    // 合并所有上下文的最大槽位
    for (const auto& context : contexts)
    {
        commandGraph.maxSlots.merge(context->resourceRequirements.maxSlots);
    }

    // 继续遍历子节点
    commandGraph.traverse(*this);
}

// 访问辅助命令图：合并最大槽位，设置渲染通道和管道状态，然后遍历子节点
// secondaryCommandGraph: 要编译的辅助命令图
// 为每个上下文设置渲染通道和管道状态，编译后恢复之前的状态
void CompileTraversal::apply(SecondaryCommandGraph& secondaryCommandGraph)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "CompileTraversal SecondaryCommandGraph", COLOR_COMPILE);

    // 获取辅助命令图的渲染通道
    auto renderPass = secondaryCommandGraph.getRenderPass();

    for (auto& context : contexts)
    {
        // 合并最大槽位
        secondaryCommandGraph.maxSlots.merge(context->resourceRequirements.maxSlots);

        // 保存之前的状态以便遍历后恢复
        auto previousRenderPass = context->renderPass;
        auto previousDefaultPipelineStates = context->defaultPipelineStates;
        auto previousOverridePipelineStates = context->overridePipelineStates;

        // 设置渲染通道
        context->renderPass = renderPass;

        // 根据窗口或帧缓冲区设置视口状态
        if (secondaryCommandGraph.window)
        {
            mergeGraphicsPipelineStates(context->mask, context->defaultPipelineStates, ViewportState::create(secondaryCommandGraph.window->extent2D()));
        }
        else if (secondaryCommandGraph.framebuffer)
        {
            mergeGraphicsPipelineStates(context->mask, context->defaultPipelineStates, ViewportState::create(secondaryCommandGraph.framebuffer->extent2D()));
        }

        // 如果使用多重采样，添加多重采样状态
        if (renderPass)
        {
            mergeGraphicsPipelineStates(context->mask, context->overridePipelineStates, MultisampleState::create(context->renderPass->maxSamples));
        }

        // 遍历辅助命令图
        secondaryCommandGraph.traverse(*this);

        // 恢复之前的值
        context->defaultPipelineStates = previousDefaultPipelineStates;
        context->overridePipelineStates = previousOverridePipelineStates;
        context->renderPass = previousRenderPass;
    }
}

// 访问渲染图：设置视口状态和渲染通道，然后遍历子节点
// renderGraph: 要编译的渲染图
// 为每个上下文设置视口状态提示、视口状态和渲染通道，编译后恢复之前的状态
void CompileTraversal::apply(RenderGraph& renderGraph)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "CompileTraversal RenderGraph", COLOR_COMPILE);

    for (auto& context : contexts)
    {
        // 保存之前的状态以便遍历后恢复
        auto previousRenderPass = context->renderPass;
        auto previousDefaultPipelineStates = context->defaultPipelineStates;
        auto previousOverridePipelineStates = context->overridePipelineStates;

        // 如果需要，启用动态视口状态处理
        renderGraph.viewportStateHint = context->resourceRequirements.viewportStateHint;

        // 合并视口状态
        mergeGraphicsPipelineStates(context->mask, context->defaultPipelineStates, renderGraph.viewportState);

        // 设置渲染通道
        context->renderPass = renderGraph.getRenderPass();

        // 如果使用多重采样，添加多重采样状态
        if (context->renderPass)
        {
            mergeGraphicsPipelineStates(context->mask, context->overridePipelineStates, MultisampleState::create(context->renderPass->maxSamples));
        }

        // 遍历渲染图
        renderGraph.traverse(*this);

        // 恢复之前的值
        context->defaultPipelineStates = previousDefaultPipelineStates;
        context->overridePipelineStates = previousOverridePipelineStates;
        context->renderPass = previousRenderPass;
    }
}

// 访问视图：设置视图相关状态，编译视图依赖状态，然后遍历子节点
// view: 要编译的视图
// 为匹配的上下文设置视图、视图ID、掩码和视图依赖状态，编译后恢复之前的状态
void CompileTraversal::apply(View& view)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "CompileTraversal View", COLOR_COMPILE);

    for (auto& context : contexts)
    {
        // 如果上下文关联了视图，确保只在与视图匹配时应用，否则跳过此上下文
        auto context_view = context->view.ref_ptr();
        if (context_view && context_view.get() != &view) continue;

        // 保存之前的状态
        auto previous_view = context->view;
        auto previous_viewID = context->viewID;
        auto previous_mask = context->mask;
        auto previous_overridePipelineStates = context->overridePipelineStates;
        auto previous_defaultPipelineStates = context->defaultPipelineStates;

        // 设置视图相关状态
        context->view = &view;
        context->viewID = view.viewID;
        context->mask = view.mask;
        context->viewDependentState = view.viewDependentState.get();

        // 如果视图有视图依赖状态，先编译它
        if (view.viewDependentState)
        {
            view.viewDependentState->compile(*context);
        }

        // 分配视图特定的管道状态
        mergeGraphicsPipelineStates(context->mask, context->defaultPipelineStates, view.camera->viewportState);
        mergeGraphicsPipelineStates(context->mask, context->overridePipelineStates, view.overridePipelineStates);

        // 遍历视图
        view.traverse(*this);

        // 恢复之前的状态
        context->view = previous_view;
        context->viewID = previous_viewID;
        context->mask = previous_mask;
        context->defaultPipelineStates = previous_defaultPipelineStates;
        context->overridePipelineStates = previous_overridePipelineStates;
    }

    // 如果视图有视图依赖状态，也遍历它
    if (view.viewDependentState) view.viewDependentState->accept(*this);
}

// 记录命令：为所有上下文记录命令
// 返回: 如果有任何上下文记录了命令，则返回true
// 为每个上下文调用记录方法，如果至少有一个上下文记录了命令则返回true
bool CompileTraversal::record()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "CompileTraversal record", COLOR_COMPILE);

    bool recorded = false;
    for (auto& context : contexts)
    {
        if (context->record()) recorded = true;
    }
    return recorded;
}

// 等待完成：等待所有上下文的命令执行完成
// 为每个上下文调用等待完成方法，阻塞直到所有命令执行完成
void CompileTraversal::waitForCompletion()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "CompileTraversal waitForCompletion", COLOR_COMPILE);

    for (auto& context : contexts)
    {
        context->waitForCompletion();
    }
}
