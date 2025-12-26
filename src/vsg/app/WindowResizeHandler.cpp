/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/View.h>
#include <vsg/app/WindowResizeHandler.h>
#include <vsg/commands/ClearAttachments.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/vk/Context.h>
#include <vsg/vk/State.h>

using namespace vsg;

// UpdateGraphicsPipelines类的构造函数
// 创建图形管线更新访问者，用于更新窗口大小调整后的图形管线
UpdateGraphicsPipelines::UpdateGraphicsPipelines()
{
    overrideMask = MASK_ALL;  // 设置覆盖掩码为全部
}

// 访问检查
// 检查对象是否已被访问过，避免重复处理
// object: 要检查的对象
// index: 视图ID索引
// 返回值：true表示未访问过，false表示已访问过
bool UpdateGraphicsPipelines::visit(const Object* object, uint32_t index)
{
    decltype(visited)::value_type objectIndex(object, index);
    if (visited.count(objectIndex) != 0) return false;  // 已访问过
    visited.insert(objectIndex);  // 标记为已访问
    return true;
}

// 应用访问者到Object对象
// 遍历对象树
// object: 要遍历的对象
void UpdateGraphicsPipelines::apply(vsg::Object& object)
{
    object.traverse(*this);
}

// 应用访问者到BindGraphicsPipeline对象
// 释放并重新编译图形管线
// bindPipeline: 绑定图形管线对象
void UpdateGraphicsPipelines::apply(vsg::BindGraphicsPipeline& bindPipeline)
{
    if (!visit(&bindPipeline, context->viewID)) return;  // 已访问过，跳过

    auto pipeline = bindPipeline.pipeline;
    if (pipeline)
    {
        pipeline->release(context->viewID);  // 释放当前视图的管线
        pipeline->compile(*context);  // 重新编译管线
    }
}

// 应用访问者到View对象
// 设置视图上下文并继续遍历
// view: 视图对象
void UpdateGraphicsPipelines::apply(vsg::View& view)
{
    if (!visit(&view, view.viewID)) return;  // 已访问过，跳过

    // 设置视图上下文
    context->viewID = view.viewID;
    context->defaultPipelineStates.emplace_back(view.camera->viewportState);

    // 继续遍历子节点
    view.traverse(*this);

    // 恢复上下文
    context->defaultPipelineStates.pop_back();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WindowResizeHandler - 窗口大小调整处理器，用于处理窗口大小变化
//

// WindowResizeHandler类的构造函数
// 创建窗口大小调整处理器
WindowResizeHandler::WindowResizeHandler()
{
    overrideMask = MASK_ALL;  // 设置覆盖掩码为全部
}

// 缩放矩形
// 根据窗口尺寸变化缩放矩形区域
// rect: 要缩放的矩形（输入输出参数）
void WindowResizeHandler::scale_rect(VkRect2D& rect)
{
    // 计算矩形的右边缘和下边缘
    int32_t edge_x = rect.offset.x + static_cast<int32_t>(rect.extent.width);
    int32_t edge_y = rect.offset.y + static_cast<int32_t>(rect.extent.height);

    // 缩放偏移量
    rect.offset.x = scale_parameter(rect.offset.x, previous_extent.width, new_extent.width);
    rect.offset.y = scale_parameter(rect.offset.y, previous_extent.height, new_extent.height);
    // 缩放尺寸（通过缩放边缘位置并减去新的偏移量）
    rect.extent.width = static_cast<uint32_t>(scale_parameter(edge_x, previous_extent.width, new_extent.width) - rect.offset.x);
    rect.extent.height = static_cast<uint32_t>(scale_parameter(edge_y, previous_extent.height, new_extent.height) - rect.offset.y);
}

// 缩放视口
// 根据窗口尺寸变化缩放视口
// viewport: 要缩放的视口（输入输出参数）
void WindowResizeHandler::scale_viewport(VkViewport& viewport)
{
    // 计算缩放比例
    float scale_x = static_cast<float>(new_extent.width) / static_cast<float>(previous_extent.width);
    float scale_y = static_cast<float>(new_extent.height) / static_cast<float>(previous_extent.height);

    // 缩放视口的位置和尺寸
    viewport.x *= scale_x;
    viewport.y *= scale_y;
    viewport.width *= scale_x;
    viewport.height *= scale_y;
}

// 访问检查
// 检查对象是否已被访问过，避免重复处理
// object: 要检查的对象
// index: 视图ID索引
// 返回值：true表示未访问过，false表示已访问过
bool WindowResizeHandler::visit(const Object* object, uint32_t index)
{
    decltype(visited)::value_type objectIndex(object, index);
    if (visited.count(objectIndex) != 0) return false;  // 已访问过
    visited.insert(objectIndex);  // 标记为已访问
    return true;
}

// 应用访问者到BindGraphicsPipeline对象
// 检查图形管线是否需要重新生成（如果没有视口状态）
// bindPipeline: 绑定图形管线对象
void WindowResizeHandler::apply(vsg::BindGraphicsPipeline& bindPipeline)
{
    if (!context) return;  // 没有上下文，跳过

    GraphicsPipeline* graphicsPipeline = bindPipeline.pipeline;

    if (!visit(graphicsPipeline, context->viewID))
    {
        return;  // 已访问过，跳过
    }

    if (graphicsPipeline)
    {
        // 检查管线是否包含视口状态
        struct ContainsViewport : public ConstVisitor
        {
            bool foundViewport = false;
            void apply(const ViewportState&) override { foundViewport = true; }
            bool operator()(const GraphicsPipeline& gp)
            {
                for (auto& pipelineState : gp.pipelineStates)
                {
                    pipelineState->accept(*this);
                }
                return foundViewport;
            }
        } containsViewport;

        // 如果管线不包含视口状态，需要重新生成
        bool needToRegenerateGraphicsPipeline = !containsViewport(*graphicsPipeline);
        if (needToRegenerateGraphicsPipeline)
        {
            graphicsPipeline->release(context->viewID);  // 释放当前视图的管线
            graphicsPipeline->compile(*context);  // 重新编译管线
        }
    }
}

// 应用访问者到Object对象
// 遍历对象树
// object: 要遍历的对象
void WindowResizeHandler::apply(vsg::Object& object)
{
    object.traverse(*this);
}

// 应用访问者到ClearAttachments对象
// 缩放清除附件中的矩形区域
// clearAttachments: 清除附件对象
void WindowResizeHandler::apply(ClearAttachments& clearAttachments)
{
    if (!visit(&clearAttachments)) return;  // 已访问过，跳过

    // 缩放所有清除矩形
    for (auto& clearRect : clearAttachments.rects)
    {
        auto& rect = clearRect.rect;
        scale_rect(rect);
    }
}

// 应用访问者到View对象
// 更新视图的投影矩阵、视口和裁剪区域
// view: 视图对象
void WindowResizeHandler::apply(vsg::View& view)
{
    if (!visit(&view)) return;  // 已访问过，跳过

    // 如果没有相机，直接遍历子节点
    if (!view.camera)
    {
        view.traverse(*this);
        return;
    }

    // 更新投影矩阵的尺寸
    view.camera->projectionMatrix->changeExtent(previous_extent, new_extent);

    // 如果有视口状态，更新视口和裁剪区域
    if (auto viewportState = view.camera->viewportState)
    {
        size_t num_viewports = std::min(viewportState->viewports.size(), viewportState->scissors.size());
        for (size_t i = 0; i < num_viewports; ++i)
        {
            auto& viewport = viewportState->viewports[i];
            auto& scissor = viewportState->scissors[i];

            // 检查渲染区域是否与裁剪区域匹配
            bool renderAreaMatches = (renderArea.offset.x == scissor.offset.x) && (renderArea.offset.y == scissor.offset.y) &&
                                     (renderArea.extent.width == scissor.extent.width) && (renderArea.extent.height == scissor.extent.height);

            // 如果尺寸变化，缩放裁剪区域
            if (new_extent != scissor.extent) scale_rect(scissor);

            // 根据裁剪区域更新视口
            viewport.x = static_cast<float>(scissor.offset.x);
            viewport.y = static_cast<float>(scissor.offset.y);
            viewport.width = static_cast<float>(scissor.extent.width);
            viewport.height = static_cast<float>(scissor.extent.height);

            // 如果之前匹配，更新渲染区域
            if (renderAreaMatches)
            {
                renderArea = scissor;
            }
        }

        // 如果有上下文，设置视图上下文并继续遍历
        if (context)
        {
            uint32_t previous_viewID = context->viewID;
            context->viewID = view.viewID;
            context->defaultPipelineStates.emplace_back(viewportState);

            view.traverse(*this);

            // 恢复上下文
            context->defaultPipelineStates.pop_back();
            context->viewID = previous_viewID;
        }
        else
        {
            view.traverse(*this);
        }
    }
    else
    {
        view.traverse(*this);
    }
}
