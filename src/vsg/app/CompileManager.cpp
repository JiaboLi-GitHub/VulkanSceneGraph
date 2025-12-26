/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/CompileManager.h>
#include <vsg/app/RecordAndSubmitTask.h>
#include <vsg/app/View.h>
#include <vsg/app/Viewer.h>
#include <vsg/core/Exception.h>
#include <vsg/io/Logger.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/utils/ShaderSet.h>

using namespace vsg;

// 重置编译结果
// 将所有字段重置为初始状态
void CompileResult::reset()
{
    result = VK_INCOMPLETE;
    maxSlots = {};
    containsPagedLOD = false;
    views.clear();
    dynamicData.clear();
}

// 合并另一个编译结果到当前结果
// cr: 要合并的编译结果
// 合并结果状态、最大槽位、分页LOD标志、视图详情和动态数据
void CompileResult::add(const CompileResult& cr)
{
    // 如果当前结果是未完成或成功，则更新为另一个结果的状态
    if (result == VK_INCOMPLETE || result == VK_SUCCESS)
    {
        result = cr.result;
    }

    // 合并最大槽位（取最大值）
    maxSlots.merge(cr.maxSlots);

    // 如果当前不包含分页LOD，则更新标志
    if (!containsPagedLOD) containsPagedLOD = cr.containsPagedLOD;

    // 合并视图详情（索引和bin集合）
    for (auto& [src_view, src_binDetails] : cr.views)
    {
        if (src_binDetails.indices.empty() && src_binDetails.bins.empty()) break;

        auto& binDetails = views[src_view];
        binDetails.indices.insert(src_binDetails.indices.begin(), src_binDetails.indices.end());
        binDetails.bins.insert(src_binDetails.bins.begin(), src_binDetails.bins.end());
    }

    // 合并动态数据
    dynamicData.add(dynamicData);
}

// 检查是否需要更新查看器
// 返回: 如果编译完成且有动态数据或视图需要更新，则返回true
// 用于判断编译后是否需要更新查看器的状态
bool CompileResult::requiresViewerUpdate() const
{
    // 如果编译未完成，不需要更新
    if (result == VK_INCOMPLETE) return false;

    // 如果有动态数据，需要更新
    if (dynamicData) return true;

    // 如果有视图的索引或bin需要更新，需要更新
    for (auto& [view, binDetails] : views)
    {
        if (!binDetails.indices.empty() || !binDetails.bins.empty()) return true;
    }
    return false;
}

// 构造函数：初始化编译管理器
// viewer: 查看器对象，用于获取状态和配置
// hints: 资源提示，用于配置资源需求
// 创建编译遍历器池，并初始化一个编译遍历器
CompileManager::CompileManager(Viewer& viewer, ref_ptr<ResourceHints> hints)
{
    // 创建编译遍历器池（使用查看器的状态）
    compileTraversals = CompileTraversals::create(viewer.status);

    // 从资源提示创建资源需求
    ResourceRequirements requirements(hints);

    // 创建编译遍历器并添加到池中
    auto ct = CompileTraversal::create(viewer, requirements);
    compileTraversals->add(ct);
#if 0
    // 可选：创建多个编译遍历器以支持并行编译（当前未启用）
    compileTraversals->add(CompileTraversal::create(*ct));
    compileTraversals->add(CompileTraversal::create(*ct));
    compileTraversals->add(CompileTraversal::create(*ct));
    numCompileTraversals = 4;
#else
    // 当前使用单个编译遍历器
    numCompileTraversals = 1;
#endif
}

// 从池中获取指定数量的编译遍历器
// count: 要获取的编译遍历器数量
// 返回: 编译遍历器容器
// 从池中取出可用的编译遍历器，如果池中没有足够的遍历器则提前返回
CompileManager::CompileTraversals::container_type CompileManager::takeCompileTraversals(size_t count)
{
    CompileTraversals::container_type cts;
    while (cts.size() < count)
    {
        // 从池中取出一个可用的编译遍历器
        auto ct = compileTraversals->take_when_available();
        if (ct)
            cts.push_back(ct);
        else
            break;  // 如果没有可用的遍历器，停止获取
    }

    return cts;
}

// 添加设备到编译遍历器
// device: Vulkan设备对象
// resourceRequirements: 资源需求配置
// 为每个编译遍历器添加设备和资源需求，然后将其返回池中
void CompileManager::add(ref_ptr<Device> device, const ResourceRequirements& resourceRequirements)
{
    auto cts = takeCompileTraversals(numCompileTraversals);
    for (auto& ct : cts)
    {
        // 为编译遍历器添加设备和资源需求
        ct->add(device, resourceRequirements);

        // 将编译遍历器返回池中
        compileTraversals->add(ct);
    }
}

// 添加窗口和视口到编译遍历器
// window: 窗口对象
// viewport: 视口状态
// resourceRequirements: 资源需求配置
// 为每个编译遍历器添加窗口、视口和资源需求，然后将其返回池中
void CompileManager::add(Window& window, ref_ptr<ViewportState> viewport, const ResourceRequirements& resourceRequirements)
{
    auto cts = takeCompileTraversals(numCompileTraversals);
    for (auto& ct : cts)
    {
        ct->add(window, viewport, resourceRequirements);

        compileTraversals->add(ct);
    }
}

// 添加窗口和视图到编译遍历器
// window: 窗口对象
// view: 视图对象
// resourceRequirements: 资源需求配置
// 为每个编译遍历器添加窗口、视图和资源需求，然后将其返回池中
void CompileManager::add(Window& window, ref_ptr<View> view, const ResourceRequirements& resourceRequirements)
{
    auto cts = takeCompileTraversals(numCompileTraversals);
    for (auto& ct : cts)
    {
        ct->add(window, view, resourceRequirements);

        compileTraversals->add(ct);
    }
}

// 添加帧缓冲区和视图到编译遍历器
// framebuffer: 帧缓冲区对象
// view: 视图对象
// resourceRequirements: 资源需求配置
// 为每个编译遍历器添加帧缓冲区、视图和资源需求，然后将其返回池中
void CompileManager::add(Framebuffer& framebuffer, ref_ptr<View> view, const ResourceRequirements& resourceRequirements)
{
    auto cts = takeCompileTraversals(numCompileTraversals);
    for (auto& ct : cts)
    {
        ct->add(framebuffer, view, resourceRequirements);

        compileTraversals->add(ct);
    }
}

// 添加查看器到编译遍历器
// viewer: 查看器对象
// resourceRequirements: 资源需求配置
// 为每个编译遍历器添加查看器的所有窗口和视图，然后将其返回池中
void CompileManager::add(const Viewer& viewer, const ResourceRequirements& resourceRequirements)
{
    auto cts = takeCompileTraversals(numCompileTraversals);
    for (auto& ct : cts)
    {
        ct->add(viewer, resourceRequirements);

        compileTraversals->add(ct);
    }
}

// 为编译遍历器分配性能分析工具
// in_instrumentation: 性能分析工具对象
// 为每个编译遍历器分配性能分析工具，然后将其返回池中
void CompileManager::assignInstrumentation(ref_ptr<Instrumentation> in_instrumentation)
{
    auto cts = takeCompileTraversals(numCompileTraversals);
    for (auto& ct : cts)
    {
        ct->assignInstrumentation(in_instrumentation);

        compileTraversals->add(ct);
    }
}

// 编译对象
// object: 要编译的场景图对象
// contextSelection: 可选的上下文选择函数，用于过滤要使用的编译上下文
// 返回: 编译结果，包含编译状态、最大槽位、视图详情和动态数据
// 收集资源需求，使用编译遍历器编译对象，并记录和提交命令（如果需要）
CompileResult CompileManager::compile(ref_ptr<Object> object, ContextSelectionFunction contextSelection)
{
    vsg::debug("CompileManager::compile(", object, ", ..)");

    // 收集资源需求
    CollectResourceRequirements collectRequirements;
    object->accept(collectRequirements);

    auto& requirements = collectRequirements.requirements;
    auto& viewDetailsStack = requirements.viewDetailsStack;

    // 初始化编译结果
    CompileResult result;
    result.maxSlots = requirements.maxSlots;
    result.containsPagedLOD = requirements.containsPagedLOD;
    result.views = requirements.views;
    result.dynamicData = requirements.dynamicData;

    // 从池中获取一个可用的编译遍历器
    auto compileTraversal = compileTraversals->take_when_available();

    // 如果没有可用的编译遍历器，中止编译
    if (!compileTraversal) return result;

    // 定义编译遍历的lambda函数
    auto run_compile_traversal = [&]() -> void {
        try
        {
            // 为每个上下文预留资源并更新结果视图
            for (auto& context : compileTraversal->contexts)
            {
                ref_ptr<View> view = context->view;

                if (view)
                {
                    // 添加视图详情到结果
                    result.views[view].add(viewDetailsStack.top());
                    // 如果视图有阴影贴图，也添加阴影视图
                    if (view->viewDependentState)
                    {
                        for (auto& sm : view->viewDependentState->shadowMaps)
                        {
                            if (sm.view)
                            {
                                result.views[sm.view].add(requirements.viewDetailsStack.top());
                            }
                        }
                    }
                }
                // 为上下文预留资源
                context->reserve(requirements);
            }

            // 使用编译遍历器遍历对象进行编译
            object->accept(*compileTraversal);

            //debug("Finished compile traversal ", object);

            // 如果需要，记录命令并提交到队列
            if (compileTraversal->record())
            {
                compileTraversal->waitForCompletion();
            }
        }
        catch (const vsg::Exception& ve)
        {
            // 捕获VSG异常并记录错误信息
            vsg::debug("CompileManager::compile() exception caught : ", ve.message);
            result.message = ve.message;
            result.result = ve.result;
        }
        catch (...)
        {
            // 捕获其他异常
            vsg::debug("CompileManager::compile() exception caught");
            result.message = "Exception occurred during compilation.";
            result.result = VK_ERROR_UNKNOWN;
        }

        debug("Finished waiting for compile ", object);
    };

    // 假设成功，失败时会被覆盖
    result.result = VK_SUCCESS;

    // 如果提供了上下文选择函数，则过滤上下文
    if (contextSelection)
    {
        std::list<ref_ptr<Context>> contexts;

        // 根据选择函数过滤上下文
        for (auto& context : compileTraversal->contexts)
        {
            if (contextSelection(*context)) contexts.push_back(context);
        }

        // 临时替换上下文列表
        compileTraversal->contexts.swap(contexts);

        run_compile_traversal();

        // 恢复原始上下文列表
        compileTraversal->contexts.swap(contexts);
    }
    else
    {
        // 使用所有上下文进行编译
        run_compile_traversal();
    }

    // 将编译遍历器返回池中
    compileTraversals->add(compileTraversal);

    return result;
}

// 编译任务
// task: 记录和提交任务对象
// resourceRequirements: 资源需求配置
// 返回: 编译结果（当前返回空结果）
// 为任务创建编译遍历器，编译所有命令图，并记录和提交命令（如果需要）
CompileResult CompileManager::compileTask(ref_ptr<RecordAndSubmitTask> task, const ResourceRequirements& resourceRequirements)
{
    // 为任务设备创建编译遍历器
    auto compileTraversal = CompileTraversal::create(task->device, resourceRequirements);

    // 如果资源需求指定使用传输任务，则为每个上下文分配传输任务
    for (const auto& context : compileTraversal->contexts)
    {
        if (resourceRequirements.dataTransferHint == COMPILE_TRAVERSAL_USE_TRANSFER_TASK)
        {
            context->transferTask = task->transferTask;
        }
    }

    // 编译所有命令图
    for (auto& cg : task->commandGraphs)
    {
        cg->accept(*compileTraversal);
    }

    // 如果需要，记录命令并等待完成
    if (compileTraversal->record())
    {
        compileTraversal->waitForCompletion();
    }

    return {};
}
