/* <editor-fold desc="MIT License">

Copyright(c) 2025 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/View.h>
#include <vsg/state/ResourceHints.h>
#include <vsg/vk/State.h>

using namespace vsg;

// 构造函数：创建状态对象
// in_maxSlots: 最大槽位
// 状态对象管理命令缓冲区记录过程中的渲染状态栈
State::State(const Slots& in_maxSlots) :
    dirty(false)
{
    reserve(in_maxSlots);
}

// 预留状态栈
// in_maxSlots: 最大槽位
// 根据最大槽位预留状态栈空间
void State::reserve(const Slots& in_maxSlots)
{
    maxSlots = in_maxSlots;
    activeMaxStateSlot = maxSlots.max();

    size_t required_size = static_cast<size_t>(activeMaxStateSlot) + 1;
    if (required_size > stateStacks.size()) stateStacks.resize(required_size);

    //    info("State::reserve(", maxStateSlot, ", ", maxViewSlot, ")");
}

// 重置状态
// 重置所有状态栈
void State::reset()
{
    for (auto& stateStack : stateStacks)
    {
        stateStack.reset();
    }

    activeMaxStateSlot = maxSlots.max();
}

// 连接命令缓冲区
// commandBuffer: 命令缓冲区对象
// 将状态对象连接到命令缓冲区，并标记状态栈为脏
void State::connect(ref_ptr<CommandBuffer> commandBuffer)
{
    _commandBuffer = commandBuffer;
    commandBuffer->state = this;
    dirtyStateStacks();
}

// 推送视图状态命令
// command: 状态命令对象
// 将状态命令推送到对应的状态栈
void State::pushView(ref_ptr<StateCommand> command)
{
    stateStacks[command->slot].push(command);
    activeMaxStateSlot = maxSlots.max();
}

// 弹出视图状态命令
// command: 状态命令对象
// 从对应的状态栈弹出状态命令
void State::popView(ref_ptr<StateCommand> command)
{
    stateStacks[command->slot].pop();
    activeMaxStateSlot = maxSlots.max();
}

// 推送视图
// view: 视图对象
// 如果使用动态视口状态，推送视图的视口状态
void State::pushView(const View& view)
{
    //info("State::pushView(View&, ", &view, ")");
    if ((viewportStateHint & DYNAMIC_VIEWPORTSTATE) && view.camera && view.camera->viewportState) pushView(view.camera->viewportState);
}

// 弹出视图
// view: 视图对象
// 如果使用动态视口状态，弹出视图的视口状态
void State::popView(const View& view)
{
    //info("State::popView(View&, ", &view, ")");
    if ((viewportStateHint & DYNAMIC_VIEWPORTSTATE) && view.camera && view.camera->viewportState) popView(view.camera->viewportState);
}

// 继承状态
// state: 源状态对象
// 从另一个状态对象继承设置（根据继承掩码）
void State::inherit(const State& state)
{
    reserve(state.maxSlots);

    reset();

    dirty = true;

    // 继承视图设置
    if ((inheritanceMask & InheritanceMask::INHERIT_VIEW_SETTINGS) != 0)
    {
        inheritViewForLODScaling = state.inheritViewForLODScaling;
        inheritedProjectionMatrix = state.inheritedProjectionMatrix;
        inheritedViewMatrix = state.inheritedViewMatrix;
        inheritedViewTransform = state.inheritedViewTransform;
    }

    // 继承状态栈
    if ((inheritanceMask & InheritanceMask::INHERIT_STATE) != 0)
    {
        stateStacks = state.stateStacks;
    }

    // 继承视口状态提示
    if ((inheritanceMask & InheritanceMask::INHERIT_VIEWPORT_STATE_HINT) != 0)
    {
        viewportStateHint = state.viewportStateHint;
    }

    // 继承矩阵
    if ((inheritanceMask & InheritanceMask::INHERIT_MATRICES) != 0)
    {
        projectionMatrixStack = state.projectionMatrixStack;
        modelviewMatrixStack = state.modelviewMatrixStack;

        _frustumUnit = state._frustumUnit;
        _frustumProjected = state._frustumProjected;
        _frustumStack = state._frustumStack;
    }
}
