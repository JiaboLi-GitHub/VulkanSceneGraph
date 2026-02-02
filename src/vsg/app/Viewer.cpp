/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/CompileTraversal.h>
#include <vsg/app/View.h>
#include <vsg/app/Viewer.h>
#include <vsg/io/Logger.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/state/Descriptor.h>

#include <chrono>
#include <map>
#include <set>

using namespace vsg;

#if VK_HEADER_VERSION < 106
#    define VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT VkResult(-1000255000)
#endif

Viewer::Viewer() :
    updateOperations(UpdateOperations::create()),
    animationManager(AnimationManager::create()),
    status(vsg::ActivityStatus::create()),
    _firstFrame(true),
    _start_point(clock::now()),
    _frameStamp(FrameStamp::create(_start_point, 0, 0.0))
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer constructor", COLOR_VIEWER);
}

Viewer::~Viewer()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer destructor", COLOR_VIEWER);

    stopThreading();

    // don't destroy viewer while devices are still active
    Viewer::deviceWaitIdle();
}

void Viewer::deviceWaitIdle() const
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer deviceWaitIdle", COLOR_VIEWER);

    std::set<VkDevice> devices;
    for (auto& window : _windows)
    {
        if (window->getDevice()) devices.insert(*(window->getDevice()));
    }

    for (const auto& task : recordAndSubmitTasks)
    {
        for (auto& cg : task->commandGraphs)
        {
            devices.insert(*(cg->device));
        }
    }

    for (auto& device : devices)
    {
        vkDeviceWaitIdle(device);
    }
}

void Viewer::addWindow(ref_ptr<Window> window)
{
    // make sure the addition is unique
    auto itr = std::find(_windows.begin(), _windows.end(), window);
    if (itr != _windows.end()) return;

    _windows.push_back(window);
}

void Viewer::removeWindow(ref_ptr<Window> window)
{
    auto itr = std::find(_windows.begin(), _windows.end(), window);
    if (itr == _windows.end()) return;

    _windows.erase(itr);

    // create a new list of CommandGraphs not associated with removed window
    CommandGraphs commandGraphs;
    for (const auto& task : recordAndSubmitTasks)
    {
        for (auto& cg : task->commandGraphs)
        {
            if (cg->window != window) commandGraphs.push_back(cg);
        }
    }

    // assign the remaining commandGraphs
    assignRecordAndSubmitTaskAndPresentation(commandGraphs);
}

void Viewer::close()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer close", COLOR_VIEWER);

    _close = true;
    status->set(false);

    stopThreading();
}

bool Viewer::active() const
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer active", COLOR_VIEWER);

    bool viewerIsActive = !_close;
    if (viewerIsActive)
    {
        for (auto window : _windows)
        {
            if (!window->valid()) viewerIsActive = false;
        }
    }

    if (!viewerIsActive)
    {
        // don't exit mainloop while any devices are still active
        deviceWaitIdle();
        return false;
    }
    else
    {
        return true;
    }
}

bool Viewer::pollEvents(bool discardPreviousEvents)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer pollEvents", COLOR_UPDATE);

    bool result = false;

    if (discardPreviousEvents) _events.clear();
    for (auto& window : _windows)
    {
        if (window->pollEvents(_events)) result = true;
    }

    return result;
}

/**
 * @brief 推进到下一帧的核心方法
 * @param simulationTime 模拟时间（传入UseTimeSinceStartPoint则自动计算从启动到当前的时间）
 * @return bool 推进成功返回true，失败（如窗口关闭、非活跃状态）返回false
 * 
 * 该函数是Viewer帧循环的核心，主要职责：
 * 1. 完成上一帧的性能分析收尾
 * 2. 检查Viewer活跃状态并处理窗口事件
 * 3. 获取下一帧的交换链图像（acquireNextImage）
 * 4. 创建当前帧的时间戳（FrameStamp），记录帧号和模拟时间
 * 5. 启动当前帧的性能分析
 * 6. 推进所有记录提交任务到当前帧
 * 7. 生成帧事件并返回推进结果
 */
bool Viewer::advanceToNextFrame(double simulationTime)
{
    // 定义帧级别的性能分析源位置信息：包含方法名、文件、行号、颜色等元数据
    static constexpr SourceLocation s_frame_source_location{"Viewer advanceToNextFrame", VsgFunctionName, __FILE__, __LINE__, COLOR_VIEWER, 1};

    // 如果启用了性能分析且上一帧的时间戳存在，标记上一帧分析结束
    if (instrumentation && _frameStamp) instrumentation->leaveFrame(&s_frame_source_location, frameReference, *_frameStamp);

    // 检查Viewer是否处于活跃状态（如窗口未关闭、未暂停），非活跃则返回false
    if (!active())
    {
        return false;
    }

    // 轮询所有窗口的事件（如鼠标、键盘、窗口大小变化等）
    // 参数true表示强制轮询，确保所有待处理事件都被处理
    pollEvents(true);

    // 获取下一帧的交换链图像（acquireNextImage），失败则返回false（如窗口最小化、交换链失效）
    if (!acquireNextFrame()) return false;

    // 获取当前系统时间，用于创建帧时间戳
    auto time = vsg::clock::now();

    // 处理第一帧的特殊初始化逻辑
    if (_firstFrame)
    {
        // 标记第一帧已处理，后续帧进入常规逻辑
        _firstFrame = false;

        // 如果传入的模拟时间为默认值（UseTimeSinceStartPoint），则初始化为0.0
        if (simulationTime == UseTimeSinceStartPoint) simulationTime = 0.0;

        // 创建第一帧的时间戳：记录当前时间、帧号0、初始模拟时间
        _frameStamp = FrameStamp::create(time, 0, simulationTime);
    }
    else
    {
        // 非第一帧：递增帧号并计算模拟时间
        if (simulationTime == UseTimeSinceStartPoint)
        {
            // 自动计算从启动点到当前的时间（秒）作为模拟时间
            simulationTime = std::chrono::duration<double, std::chrono::seconds::period>(time - _start_point).count();
        }

        // 创建当前帧的时间戳：当前时间、上一帧号+1、计算后的模拟时间
        _frameStamp = FrameStamp::create(time, _frameStamp->frameCount + 1, simulationTime);
    }

    // 如果启用了性能分析，标记当前帧分析开始
    if (instrumentation) instrumentation->enterFrame(&s_frame_source_location, frameReference, *_frameStamp);

    // 推进所有记录提交任务到当前帧（更新任务的帧状态、命令缓冲区等）
    for (auto& task : recordAndSubmitTasks)
    {
        task->advance();
    }

    // 创建当前帧的事件并添加到事件队列，供后续处理（如场景更新、逻辑处理）
    _events.emplace_back(new FrameEvent(_frameStamp));

    // 帧推进成功，返回true
    return true;
}

bool Viewer::acquireNextFrame()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer acquireNextFrame", COLOR_VIEWER);

    if (_close) return false;

    VkResult result = VK_SUCCESS;

    for (auto& window : _windows)
    {
        if (!window->visible()) continue;

        while ((result = window->acquireNextImage()) != VK_SUCCESS)
        {
            if (result == VK_ERROR_SURFACE_LOST_KHR ||
                result == VK_ERROR_OUT_OF_DATE_KHR ||
                result == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT ||
                result == VK_SUBOPTIMAL_KHR)
            {
                // force a rebuild of the Swapchain by calling Window::resize();
                window->resize();
            }
            else if (result == VK_ERROR_DEVICE_LOST)
            {
                // a lost device can only be recovered by opening a new VkDevice, and success is not guaranteed.
                // not currently implemented, so exit main loop.
                warn("window->acquireNextImage() VkResult = VK_ERROR_DEVICE_LOST. Device loss can indicate invalid Vulkan API usage or driver/hardware issues.");
                break;
            }
            else
            {
                warn("window->acquireNextImage() VkResult = ", result);
                break;
            }
        }
    }

    return result == VK_SUCCESS;
}

VkResult Viewer::waitForFences(size_t relativeFrameIndex, uint64_t timeout)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer waitForFences", COLOR_VIEWER);

    VkResult result = VK_SUCCESS;
    for (auto& task : recordAndSubmitTasks)
    {
        auto fenceToWait = task->fence(relativeFrameIndex);
        if (fenceToWait)
        {
            result = fenceToWait->wait(timeout);
            if (result != VK_SUCCESS) return result;
        }
    }
    return result;
}

void Viewer::handleEvents()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer handle events", COLOR_UPDATE);

    for (auto& vsg_event : _events)
    {
        for (auto& handler : _eventHandlers)
        {
            vsg_event->accept(*handler);
        }
    }
}

/**
 * @brief Viewer编译方法：核心资源编译与初始化
 * @param hints 资源提示信息，用于指导资源分配、线程数等配置
 * 
 * 该函数完成Viewer的核心编译流程，主要职责：
 * 1. 收集所有命令图的资源需求（Descriptor、Buffer、Image等）
 * 2. 为每个View分配binID并创建对应的Bin对象
 * 3. 初始化DatabasePager（处理分页LOD资源加载）
 * 4. 创建并配置CompileManager编译Vulkan资源
 * 5. 启动DatabasePager线程处理异步资源加载
 */
void Viewer::compile(ref_ptr<ResourceHints> hints)
{
    // L1级CPU性能分析埋点 - 标记Viewer编译过程，使用编译阶段专属颜色
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer compile", COLOR_COMPILE);

    // 如果没有待处理的记录提交任务，直接返回（无资源需要编译）
    if (recordAndSubmitTasks.empty())
    {
        return;
    }

    // 标记是否包含分页LOD资源（需要异步加载的LOD节点）
    bool containsPagedLOD = false;
    // 数据库分页器：用于异步加载/卸载分页LOD资源
    ref_ptr<DatabasePager> databasePager;

    /**
     * @brief 设备资源结构体
     * 为每个Vulkan设备存储资源收集器，用于汇总该设备的所有资源需求
     */
    struct DeviceResources
    {
        // 资源收集器：遍历场景图收集所有Vulkan资源需求
        CollectResourceRequirements collectResources;
    };

    // 按设备分组的资源映射表：key=Vulkan设备，value=该设备的资源收集结果
    using DeviceResourceMap = std::map<ref_ptr<vsg::Device>, DeviceResources>;
    DeviceResourceMap deviceResourceMap;

    // 遍历所有记录提交任务，收集每个设备的资源需求
    for (auto& task : recordAndSubmitTasks)
    {
        // 获取当前任务对应设备的资源收集器（不存在则自动创建）
        auto& collectResources = deviceResourceMap[task->device].collectResources;
        // 获取资源需求结构体，用于存储收集到的所有资源信息
        auto& resourceRequirements = collectResources.requirements;

        // 如果提供了资源提示，先收集提示中的资源需求
        if (hints) hints->accept(collectResources);

        // 遍历任务中的所有命令图，收集命令图内的所有资源需求
        for (auto& commandGraph : task->commandGraphs)
        {
            commandGraph->accept(collectResources);
        }

        // 将收集到的最小暂存缓冲区大小赋值给传输任务
        // 暂存缓冲区用于CPU->GPU的数据传输（如顶点/索引数据上传）
        task->transferTask->minimumStagingBufferSize = resourceRequirements.minimumStagingBufferSize;

        // 如果任务关联了数据库分页器且全局分页器未初始化，则复用该分页器
        if (task->databasePager && !databasePager) databasePager = task->databasePager;
    }

    // 为每个设备分配DescriptorPool（描述符池）并汇总所有View信息
    // Views是一个映射表：key=View指针，value=该View的bin详情（索引、排序等）
    ResourceRequirements::Views views;
    for (auto& [device, deviceResources] : deviceResourceMap)
    {
        auto& collectResources = deviceResources.collectResources;
        auto& resourceRequirements = collectResources.requirements;

        // 将当前设备的View信息合并到全局View集合中
        views.insert(resourceRequirements.views.begin(), resourceRequirements.views.end());

        // 标记是否存在分页LOD资源（只要有一个设备包含则全局标记为true）
        if (resourceRequirements.containsPagedLOD) containsPagedLOD = true;
    }

    // 为每个View分配binID并创建对应的Bin对象（用于渲染排序）
    for (auto& [const_view, binDetails] : views)
    {
        // 去除const限制以修改View的bins属性
        auto view = const_cast<View*>(const_view);

        // 遍历该View需要的所有bin编号
        for (auto& binNumber : binDetails.indices)
        {
            // 检查该bin编号是否已存在于View的bins中
            bool binNumberMatched = false;
            for (const auto& bin : view->bins)
            {
                if (bin->binNumber == binNumber)
                {
                    binNumberMatched = true;
                    break;
                }
            }

            // 如果bin编号不存在，则创建新的Bin对象并添加到View
            if (!binNumberMatched)
            {
                // 根据bin编号确定排序规则：
                // binNumber < 0: 升序排序 | binNumber == 0: 不排序 | binNumber > 0: 降序排序
                Bin::SortOrder sortOrder = (binNumber < 0) ? Bin::ASCENDING : ((binNumber == 0) ? Bin::NO_SORT : Bin::DESCENDING);
                // 创建Bin对象并添加到View的bins列表
                view->bins.push_back(Bin::create(binNumber, sortOrder));
            }
        }
    }

    // 如果场景包含分页LOD资源但未初始化DatabasePager，则创建默认分页器
    if (containsPagedLOD && !databasePager)
    {
        databasePager = DatabasePager::create();
        // 如果启用了性能分析，为分页器分配分析器
        if (instrumentation) databasePager->assignInstrumentation(instrumentation);
    }

    // 创建Vulkan对象并配置任务的分页器
    for (const auto& task : recordAndSubmitTasks)
    {
        // 获取当前任务对应设备的资源需求
        const auto& deviceResource = deviceResourceMap[task->device];
        const auto& resourceRequirements = deviceResource.collectResources.requirements;

        // 标记当前任务是否包含分页LOD资源
        bool task_containsPagedLOD = false;

        // 为任务中的所有命令图设置最大插槽数（maxSlots），并检查是否包含分页LOD
        for (const auto& commandGraph : task->commandGraphs)
        {
            // 设置命令图的最大插槽数（用于资源分配）
            commandGraph->maxSlots = resourceRequirements.maxSlots;
            // 标记当前任务是否包含分页LOD
            if (resourceRequirements.containsPagedLOD) task_containsPagedLOD = true;
        }

        // 如果当前任务包含分页LOD且未关联分页器，则分配全局分页器
        if (task_containsPagedLOD)
        {
            if (!task->databasePager) task->databasePager = databasePager;
        }
    }

    // 初始化编译管理器（CompileManager）：负责编译Vulkan资源的核心管理器
    if (!compileManager)
    {
        // 创建编译管理器，关联当前Viewer和资源提示
        compileManager = CompileManager::create(*this, hints);
        // 如果启用了性能分析，为编译管理器分配分析器
        if (instrumentation) compileManager->assignInstrumentation(instrumentation);
    }

    // 将编译管理器分配给DatabasePager（用于异步编译加载的资源）
    if (databasePager && !databasePager->compileManager)
    {
        databasePager->compileManager = compileManager;
    }

    // 编译所有记录提交任务的资源
    for (auto& task : recordAndSubmitTasks)
    {
        // 获取当前任务对应设备的资源需求
        auto& deviceResource = deviceResourceMap[task->device];
        auto& resourceRequirements = deviceResource.collectResources.requirements;

        // 使用编译管理器编译当前任务的所有资源
        compileManager->compileTask(task, resourceRequirements);

        // 为传输任务分配动态数据（如UBO、SSBO等需要频繁更新的数据）
        task->transferTask->assign(resourceRequirements.dynamicData);
    }

    // 启动所有DatabasePager的工作线程（处理异步资源加载）
    for (const auto& task : recordAndSubmitTasks)
    {
        if (task->databasePager)
        {
            // 如果有资源提示，使用提示中指定的线程数启动；否则使用默认配置
            if (hints)
                task->databasePager->start(hints->numDatabasePagerReadThreads);
            else
                task->databasePager->start();
        }
    }
}

/**
 * @brief 分配记录提交任务并设置渲染呈现逻辑
 * @param in_commandGraphs 输入的命令图集合，包含待执行的渲染命令逻辑
 * 
 * 该函数是Viewer类的核心方法，主要负责：
 * 1. 停止当前线程并清理旧任务
 * 2. 按设备/队列族分组命令图
 * 3. 创建RecordAndSubmitTask处理命令缓冲区录制和提交
 * 4. 创建Presentation处理窗口呈现
 * 5. 重新启动线程处理任务
 */
void Viewer::assignRecordAndSubmitTaskAndPresentation(CommandGraphs in_commandGraphs)
{
    // L1级CPU性能分析埋点 - 标记Viewer的任务分配与提交过程，使用指定颜色标识
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer assignRecordAndSubmitTaskAndPresentation", COLOR_VIEWER);

    // 记录当前线程状态，用于后续恢复线程
    bool needToStartThreading = _threading;
    // 如果线程正在运行，先停止线程以确保安全清理旧任务
    if (_threading) stopThreading();

    // 查找已分配的DatabasePager（数据库分页器），用于后续复用
    ref_ptr<DatabasePager> databasePager;
    for (const auto& task : recordAndSubmitTasks)
    {
        if (task->databasePager)
        {
            databasePager = task->databasePager;
            break;
        }
    }

    // 清空旧的呈现对象和记录提交任务列表，准备创建新任务
    presentations.clear();
    recordAndSubmitTasks.clear();

    /**
     * @brief 逻辑设备队列族结构体
     * 用于分组命令图：按设备、队列族、呈现族组合来区分不同的命令执行上下文
     */
    struct DeviceQueueFamily
    {
        Device* device = nullptr; // Vulkan设备指针
        int queueFamily = -1;     // 命令执行队列族索引
        int presentFamily = -1;   // 呈现队列族索引（-1表示无呈现需求）

        // 重载小于运算符，用于std::map的键排序
        bool operator<(const DeviceQueueFamily& rhs) const
        {
            if (device < rhs.device) return true;
            if (device > rhs.device) return false;
            if (queueFamily < rhs.queueFamily) return true;
            if (queueFamily > rhs.queueFamily) return false;
            return presentFamily < rhs.presentFamily;
        }
    };

    /**
     * @brief 查找命令图中关联的窗口对象的访问器
     * 继承自Visitor（访问者模式），遍历命令图树结构收集所有窗口
     */
    struct FindWindows : public Visitor
    {
        std::set<ref_ptr<Window>> windows; // 存储找到的唯一窗口集合

        // 通用对象遍历：继续遍历对象的子节点
        void apply(Object& object) override { object.traverse(*this); }

        // 命令图遍历：提取命令图关联的窗口并继续遍历子节点
        void apply(CommandGraph& cg) override
        {
            if (cg.window) windows.insert(cg.window);
            cg.traverse(*this);
        }
    } findWindows;

    // 按设备+队列族+呈现族组合分组命令图，确保相同执行上下文的命令图在一起处理
    std::map<DeviceQueueFamily, CommandGraphs> deviceCommandGraphsMap;
    for (auto& commandGraph : in_commandGraphs)
    {
        // 遍历命令图收集关联的窗口
        commandGraph->accept(findWindows);
        // 将命令图归类到对应的设备队列族分组中
        deviceCommandGraphsMap[DeviceQueueFamily{commandGraph->device.get(), commandGraph->queueFamily, commandGraph->presentFamily}].emplace_back(commandGraph);
    }

    // 将找到的所有窗口赋值给Viewer，使Viewer能够跟踪管理这些窗口
    _windows.assign(findWindows.windows.begin(), findWindows.windows.end());

    // 为每个设备队列族分组创建对应的RecordAndSubmitTask和Presentation对象
    for (auto& [deviceQueueFamily, commandGraphs] : deviceCommandGraphsMap)
    {
        // 分离主/次命令图：确保次命令图（secondary）优先处理
        // 次命令图通常用于可复用的命令序列，需要先录制再被主命令图引用
        CommandGraphs primary_commandGraphs;   // 主命令图集合（VK_COMMAND_BUFFER_LEVEL_PRIMARY）
        CommandGraphs secondary_commandGraphs; // 次命令图集合（VK_COMMAND_BUFFER_LEVEL_SECONDARY）

        for (auto& commandGraph : commandGraphs)
        {
            if (commandGraph->level() == VK_COMMAND_BUFFER_LEVEL_PRIMARY)
                primary_commandGraphs.emplace_back(commandGraph);
            else
                secondary_commandGraphs.emplace_back(commandGraph);
        }

        // 重新组织命令图顺序：次命令图在前，主命令图在后
        if (!secondary_commandGraphs.empty())
        {
            commandGraphs = secondary_commandGraphs;
            commandGraphs.insert(commandGraphs.end(), primary_commandGraphs.begin(), primary_commandGraphs.end());
        }

        // 设置命令缓冲区数量（三重缓冲），用于减少帧等待，提升渲染流畅度
        uint32_t numBuffers = 3;

        // 获取当前分组对应的Vulkan设备
        auto device = deviceQueueFamily.device;

        // 获取用于录制和提交命令的主队列
        ref_ptr<Queue> mainQueue = device->getQueue(deviceQueueFamily.queueFamily);

        // 获取呈现队列（如果有呈现需求且支持）
        ref_ptr<Queue> presentQueue;
        if (deviceQueueFamily.presentFamily >= 0) presentQueue = device->getQueue(deviceQueueFamily.presentFamily);

        // 初始化传输队列为主队列，后续尝试寻找更合适的专用传输队列
        ref_ptr<Queue> transferQueue = mainQueue;

        // 传输队列需要的标志位：传输位 + 图形位（确保支持图像blit操作）
        VkQueueFlags transferQueueFlags = VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT;
        // 遍历设备所有队列，寻找符合要求的非主队列作为专用传输队列
        for (auto& queue : device->getQueues())
        {
            // 检查队列是否包含所需的所有标志位
            if ((queue->queueFlags() & transferQueueFlags) == transferQueueFlags)
            {
                // 优先选择非主队列，避免传输操作阻塞主渲染队列
                if (queue != mainQueue)
                {
                    transferQueue = queue;
                    break;
                }
            }
        }

        // 如果有呈现队列族（说明需要窗口呈现）
        if (deviceQueueFamily.presentFamily >= 0)
        {
            // 重新收集当前命令图分组关联的所有窗口
            findWindows.windows.clear();
            for (auto& commandGraph : commandGraphs)
            {
                commandGraph->accept(findWindows);
            }

            // 将窗口集合转换为有序容器
            Windows activeWindows(findWindows.windows.begin(), findWindows.windows.end());

            // 创建记录并提交任务：管理命令缓冲区录制、提交和同步
            auto recordAndSubmitTask = vsg::RecordAndSubmitTask::create(device, numBuffers);
            recordAndSubmitTask->commandGraphs = commandGraphs;     // 关联命令图
            recordAndSubmitTask->databasePager = databasePager;     // 关联数据库分页器
            recordAndSubmitTask->windows = activeWindows;           // 关联窗口
            recordAndSubmitTask->queue = mainQueue;                 // 设置主执行队列
            recordAndSubmitTasks.emplace_back(recordAndSubmitTask); // 添加到任务列表

            // 为传输任务设置专用传输队列
            recordAndSubmitTask->transferTask->transferQueue = transferQueue;

            // 如果启用了性能分析，为任务分配分析器
            if (instrumentation) recordAndSubmitTask->assignInstrumentation(instrumentation);

            // 创建呈现对象：管理窗口的交换链和图像呈现
            auto presentation = vsg::Presentation::create();
            presentation->windows = activeWindows;                                   // 关联窗口
            presentation->queue = device->getQueue(deviceQueueFamily.presentFamily); // 设置呈现队列
            presentations.emplace_back(presentation);                                // 添加到呈现列表
        }
        else
        {
            // 无呈现队列族（说明是后台渲染/计算任务，无窗口输出）
            // 创建记录并提交任务（仅处理命令执行，无呈现逻辑）
            auto recordAndSubmitTask = vsg::RecordAndSubmitTask::create(device, numBuffers);
            recordAndSubmitTask->commandGraphs = commandGraphs;     // 关联命令图
            recordAndSubmitTask->databasePager = databasePager;     // 关联数据库分页器
            recordAndSubmitTask->queue = mainQueue;                 // 设置主执行队列
            recordAndSubmitTasks.emplace_back(recordAndSubmitTask); // 添加到任务列表

            // 为传输任务设置专用传输队列
            recordAndSubmitTask->transferTask->transferQueue = transferQueue;

            // 如果启用了性能分析，为任务分配分析器
            if (instrumentation) recordAndSubmitTask->assignInstrumentation(instrumentation);
        }
    }

    // 如果之前线程是运行状态，重新设置并启动线程处理新任务
    if (needToStartThreading) setupThreading();
}

void Viewer::addRecordAndSubmitTaskAndPresentation(CommandGraphs commandGraphs)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer addRecordAndSubmitTaskAndPresentation", COLOR_VIEWER);

    // collect the existing CommandGraphs
    CommandGraphs combinedCommandGraphs;
    for (const auto& task : recordAndSubmitTasks)
    {
        for (auto& cg : task->commandGraphs)
        {
            combinedCommandGraphs.push_back(cg);
        }
    }

    // add the new CommandGraphs
    combinedCommandGraphs.insert(combinedCommandGraphs.end(), commandGraphs.begin(), commandGraphs.end());

    // assign the combined CommandGraphs
    assignRecordAndSubmitTaskAndPresentation(combinedCommandGraphs);
}

void Viewer::setupThreading()
{
    stopThreading();

    // check how many valid tasks there are.
    uint32_t numValidTasks = 0;
    for (const auto& task : recordAndSubmitTasks)
    {
        if (!task->commandGraphs.empty())
        {
            ++numValidTasks;
        }
    }

    // check if there is any point in setting up threading
    if (numValidTasks == 0)
    {
        return;
    }

    status->set(true);
    _threading = true;
    _frameBlock = FrameBlock::create(status);
    _submissionCompleted = Barrier::create(1 + numValidTasks);

    // set up required threads for each task
    for (auto& task : recordAndSubmitTasks)
    {
        if (task->commandGraphs.size() == 1 && !task->transferTask)
        {
            // task only contains a single CommandGraph so keep thread simple
            auto run = [](ref_ptr<RecordAndSubmitTask> viewer_task, ref_ptr<FrameBlock> viewer_frameBlock, ref_ptr<Barrier> submissionCompleted, const std::string& threadName) {
                auto local_instrumentation = shareOrDuplicateForThreadSafety(viewer_task->instrumentation);
                if (local_instrumentation) local_instrumentation->setThreadName(threadName);

                auto frameStamp = viewer_frameBlock->initial_value;

                // wait for this frame to be signaled
                while (viewer_frameBlock->wait_for_change(frameStamp))
                {
                    CPU_INSTRUMENTATION_L1_NC(local_instrumentation, "Viewer run", COLOR_RECORD);

                    viewer_task->submit(frameStamp);

                    submissionCompleted->arrive_and_drop();
                }
            };

            threads.emplace_back(run, task, _frameBlock, _submissionCompleted, make_string("Viewer run thread"));
        }
        else if (!task->commandGraphs.empty())
        {
            // we have multiple CommandGraphs in a single Task so set up a thread per CommandGraph
            struct SharedData : public Inherit<Object, SharedData>
            {
                SharedData(ref_ptr<RecordAndSubmitTask> in_task, ref_ptr<FrameBlock> in_frameBlock, ref_ptr<Barrier> in_submissionCompleted, uint32_t numThreads) :
                    task(in_task),
                    frameBlock(in_frameBlock),
                    submissionCompletedBarrier(in_submissionCompleted)
                {
                    recordedCommandBuffers = RecordedCommandBuffers::create();
                    recordStartBarrier = Barrier::create(numThreads);
                    recordCompletedBarrier = Barrier::create(numThreads);
                }

                // shared between all threads
                ref_ptr<RecordAndSubmitTask> task;
                ref_ptr<FrameBlock> frameBlock;
                ref_ptr<Barrier> submissionCompletedBarrier;

                // shared between threads associated with each task
                ref_ptr<RecordedCommandBuffers> recordedCommandBuffers;
                ref_ptr<Barrier> recordStartBarrier;
                ref_ptr<Barrier> recordCompletedBarrier;
            };

            uint32_t numThreads = static_cast<uint32_t>(task->commandGraphs.size());
            if (task->transferTask) ++numThreads;

            ref_ptr<SharedData> sharedData = SharedData::create(task, _frameBlock, _submissionCompleted, numThreads);

            auto run_primary = [](ref_ptr<SharedData> data, ref_ptr<CommandGraph> commandGraph, const std::string& threadName) {
                auto local_instrumentation = shareOrDuplicateForThreadSafety(data->task->instrumentation);
                if (local_instrumentation) local_instrumentation->setThreadName(threadName);

                auto frameStamp = data->frameBlock->initial_value;

                // wait for this frame to be signaled
                while (data->frameBlock->wait_for_change(frameStamp))
                {
                    CPU_INSTRUMENTATION_L1_NC(local_instrumentation, "Viewer primary", COLOR_RECORD);

                    // primary thread starts the task
                    data->task->start();

                    data->recordStartBarrier->arrive_and_wait();

                    //vsg::info("run_primary");

                    commandGraph->record(data->recordedCommandBuffers, frameStamp, data->task->databasePager);

                    data->recordCompletedBarrier->arrive_and_wait();

                    // primary thread finishes the task, submitting all the command buffers recorded by the primary and all secondary threads to its queue
                    data->task->finish(data->recordedCommandBuffers);

                    data->recordedCommandBuffers->clear();

                    data->submissionCompletedBarrier->arrive_and_wait();
                }
            };

            auto run_secondary = [](ref_ptr<SharedData> data, ref_ptr<CommandGraph> commandGraph, const std::string& threadName) {
                auto local_instrumentation = shareOrDuplicateForThreadSafety(data->task->instrumentation);
                if (local_instrumentation) local_instrumentation->setThreadName(threadName);

                auto frameStamp = data->frameBlock->initial_value;

                // wait for this frame to be signaled
                while (data->frameBlock->wait_for_change(frameStamp))
                {
                    CPU_INSTRUMENTATION_L1_NC(local_instrumentation, "Viewer secondary", COLOR_RECORD);

                    data->recordStartBarrier->arrive_and_wait();

                    commandGraph->record(data->recordedCommandBuffers, frameStamp, data->task->databasePager);

                    data->recordCompletedBarrier->arrive_and_wait();
                }
            };

            auto run_transfer = [](ref_ptr<SharedData> data, ref_ptr<TransferTask> transferTask, TransferTask::TransferMask transferMask, const std::string& threadName) {
                auto local_instrumentation = shareOrDuplicateForThreadSafety(data->task->instrumentation);
                if (local_instrumentation) local_instrumentation->setThreadName(threadName);

                auto frameStamp = data->frameBlock->initial_value;

                // wait for this frame to be signaled
                while (data->frameBlock->wait_for_change(frameStamp))
                {
                    CPU_INSTRUMENTATION_L1_NC(local_instrumentation, "Viewer transfer", COLOR_RECORD);

                    data->recordStartBarrier->arrive_and_wait();

                    //vsg::info("run_transfer");

                    if (auto transfer = transferTask->transferData(transferMask); transfer.result == VK_SUCCESS)
                    {
                        if (transfer.dataTransferredSemaphore)
                        {
                            data->task->earlyDataTransferredSemaphore = transfer.dataTransferredSemaphore;
                        }
                    }

                    data->recordCompletedBarrier->arrive_and_wait();
                }
            };

            for (uint32_t i = 0; i < task->commandGraphs.size(); ++i)
            {
                if (i == 0)
                    threads.emplace_back(run_primary, sharedData, task->commandGraphs[i], make_string("Viewer primary thread"));
                else
                    threads.emplace_back(run_secondary, sharedData, task->commandGraphs[i], make_string("Viewer secondary thread ", i));
            }

            if (task->transferTask)
            {
                threads.emplace_back(run_transfer, sharedData, task->transferTask, TransferTask::TRANSFER_BEFORE_RECORD_TRAVERSAL, make_string("Viewer early transferTask thread"));
            }
        }
    }
}

void Viewer::stopThreading()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer stopThreading", COLOR_VIEWER);

    if (!_threading) return;
    _threading = false;

    debug("Viewer::stopThreading()");

    // release the blocks to enable threads to exit cleanly
    // need to manually wake up the threads waiting on this frameBlock so they check the status value and exit cleanly.
    status->set(false);
    _frameBlock->wake();

    for (auto& thread : threads)
    {
        if (thread.joinable()) thread.join();
    }
    threads.clear();
}

void Viewer::update()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer update", COLOR_UPDATE);

    // merge any updates from the DatabasePager
    for (const auto& task : recordAndSubmitTasks)
    {
        if (task->databasePager)
        {
            CompileResult cr;
            task->databasePager->updateSceneGraph(_frameStamp, cr);
            if (cr.requiresViewerUpdate()) updateViewer(*this, cr);
        }
    }

    // run update operations
    updateOperations->run();

    // run aniamtions
    animationManager->run(_frameStamp);
}

void Viewer::recordAndSubmit()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer recordAndSubmitTask", COLOR_VIEWER);

    // reset connected ExecuteCommands
    for (const auto& recordAndSubmitTask : recordAndSubmitTasks)
    {
        for (auto& commandGraph : recordAndSubmitTask->commandGraphs)
        {
            commandGraph->reset();
        }
    }

#if 1
    if (_threading)
#else
    // The following is a workaround for an odd "Possible data race during write of size 1" warning that valgrind tool=helgrind reports
    // on the first call to vkBeginCommandBuffer despite them being done on independent command buffers.  This could well be a driver bug or a false positive.
    // If you want to quieten this warning then change the #if above to #if 0 as rendering the first three frames single threaded avoids the warning.
    if (_threading && _frameStamp->frameCount > 2)
#endif
    {
        _frameBlock->set(_frameStamp);
        _submissionCompleted->arrive_and_wait();
    }
    else
    {
        for (auto& recordAndSubmitTask : recordAndSubmitTasks)
        {
            recordAndSubmitTask->submit(_frameStamp);
        }
    }
}

void Viewer::present()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "Viewer present", COLOR_VIEWER);

    for (auto& presentation : presentations)
    {
        presentation->present();
    }
}

void Viewer::assignInstrumentation(ref_ptr<Instrumentation> in_instrumentation)
{
    bool previous_threading = _threading;
    if (_threading) stopThreading();

    // don't change Instrumentation while devices are still active
    Viewer::deviceWaitIdle();

    instrumentation = in_instrumentation;

    // assign instrumentation after settings up recordAndSubmitTasks, but before compile() to allow compile to initialize the Instrumentation with the approach queue etc.
    for (auto& task : recordAndSubmitTasks)
    {
        task->assignInstrumentation(instrumentation);
    }

    if (compileManager) compileManager->assignInstrumentation(instrumentation);

    if (animationManager) animationManager->assignInstrumentation(instrumentation);

    if (previous_threading) setupThreading();
}

void vsg::updateViewer(Viewer& viewer, const CompileResult& compileResult)
{
    CPU_INSTRUMENTATION_L1_NC(viewer.instrumentation, "updateViewer", COLOR_VIEWER);

    updateTasks(viewer.recordAndSubmitTasks, viewer.compileManager, compileResult);
}
