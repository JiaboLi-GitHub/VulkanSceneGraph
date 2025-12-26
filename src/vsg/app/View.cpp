/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/View.h>
#include <vsg/nodes/Bin.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/utils/ShaderSet.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// 线程安全的容器，用于管理每个vsg::View的视图ID
static std::mutex s_ViewCountMutex;  // 视图计数互斥锁
static std::vector<uint32_t> s_ActiveViews;  // 活动视图计数向量

// 获取唯一的视图ID
// 从活动视图列表中查找空闲的视图ID，如果没有则创建新的
// 返回值：唯一的视图ID
static uint32_t getUniqueViewID()
{
    std::scoped_lock<std::mutex> guard(s_ViewCountMutex);

    uint32_t viewID = 0;
    // 查找空闲的视图ID（计数为0）
    for (viewID = 0; viewID < static_cast<uint32_t>(s_ActiveViews.size()); ++viewID)
    {
        if (s_ActiveViews[viewID] == 0)
        {
            ++s_ActiveViews[viewID];  // 增加计数
            return viewID;
        }
    }

    // 没有空闲的，创建新的视图ID
    s_ActiveViews.push_back(1);

    return viewID;
}

// 共享视图ID
// 增加指定视图ID的引用计数
// viewID: 要共享的视图ID
// 返回值：视图ID
static uint32_t sharedViewID(uint32_t viewID)
{
    std::scoped_lock<std::mutex> guard(s_ViewCountMutex);

    // 如果视图ID在范围内，增加计数
    if (viewID < static_cast<uint32_t>(s_ActiveViews.size()))
    {
        ++s_ActiveViews[viewID];
        return viewID;
    }

    // 视图ID超出范围，创建新的
    viewID = static_cast<uint32_t>(s_ActiveViews.size());
    s_ActiveViews.push_back(1);

    return viewID;
}

// 释放视图ID
// 减少指定视图ID的引用计数
// viewID: 要释放的视图ID
static void releaseViewID(uint32_t viewID)
{
    std::scoped_lock<std::mutex> guard(s_ViewCountMutex);
    --s_ActiveViews[viewID];  // 减少计数
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// View - 视图类，用于管理渲染视图
//

// View类的构造函数（仅特性）
// 创建视图对象，使用指定的视图特性
// in_features: 视图特性标志
View::View(ViewFeatures in_features) :
    viewID(getUniqueViewID()),  // 获取唯一的视图ID
    features(in_features)  // 视图特性
{
    viewDependentState = ViewDependentState::create(this);  // 创建视图相关状态
}

// View类的拷贝构造函数
// 从另一个视图对象复制视图
// view: 要复制的视图对象
View::View(const View& view) :
    Inherit(view),
    viewID(sharedViewID(view.viewID)),  // 共享视图ID（增加引用计数）
    features(view.features),  // 复制特性
    mask(view.mask),  // 复制掩码
    LODScale(view.LODScale)  // 复制LOD缩放
{
    // 如果有相机和视口状态，创建新的相机并复制视口状态
    if (view.camera && view.camera->viewportState)
    {
        camera = vsg::Camera::create();
        camera->viewportState = view.camera->viewportState;
    }

    viewDependentState = ViewDependentState::create(this);  // 创建视图相关状态

    // info("View::View(const View&) ", this, ", ", viewDependentState, ", ", viewID);
}

// View类的构造函数（相机和场景图）
// 使用指定的相机和场景图创建视图
// in_camera: 相机对象
// in_scenegraph: 场景图节点
// in_features: 视图特性标志
View::View(ref_ptr<Camera> in_camera, ref_ptr<Node> in_scenegraph, ViewFeatures in_features) :
    camera(in_camera),  // 相机对象
    viewID(getUniqueViewID()),  // 获取唯一的视图ID
    features(in_features)  // 视图特性
{
    if (in_scenegraph) addChild(in_scenegraph);  // 添加场景图子节点

    viewDependentState = ViewDependentState::create(this);  // 创建视图相关状态

    // info("View::View(ref_ptr<Camera> in_camera) ", this, ", ", viewDependentState, ", ", viewID);
}

// View类的析构函数
// 释放视图ID和清理视图相关状态
View::~View()
{
    if (viewDependentState) viewDependentState->view = nullptr;  // 清除视图相关状态的视图引用
    releaseViewID(viewID);  // 释放视图ID
}

// 共享视图
// 与另一个视图共享视图ID和设置
// view: 要共享的视图对象
void View::share(const View& view)
{
    // 如果视图ID不同，释放当前ID并共享新的ID
    if (viewID != view.viewID)
    {
        releaseViewID(viewID);
        const_cast<uint32_t&>(viewID) = sharedViewID(view.viewID);
    }

    // 复制掩码
    mask = view.mask;
    // 如果有相机和视口状态，创建或更新相机
    if (view.camera && view.camera->viewportState)
    {
        if (!camera) camera = vsg::Camera::create();
        camera->viewportState = view.camera->viewportState;
    }
}
