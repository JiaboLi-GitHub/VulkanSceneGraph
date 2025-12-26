/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/AnimationManager.h>

using namespace vsg;

// AnimationManager类的默认构造函数
// 创建动画管理器，用于管理和更新所有活动的动画
AnimationManager::AnimationManager()
{
}

// 分配性能分析工具
// 设置用于性能分析的Instrumentation对象
// in_instrumentation: 性能分析工具对象
void AnimationManager::assignInstrumentation(ref_ptr<Instrumentation> in_instrumentation)
{
    instrumentation = in_instrumentation;
}

// 播放动画
// 启动指定的动画并将其添加到活动动画列表
// animation: 要播放的动画
// startTime: 动画起始时间
// 返回值：true表示成功启动，false表示启动失败
bool AnimationManager::play(vsg::ref_ptr<vsg::Animation> animation, double startTime)
{
    CPU_INSTRUMENTATION_L2_NC(instrumentation, "AnimationManager play animation", COLOR_VIEWER);

    // 检查动画是否已经激活
    bool already_active = animation->active();
    // 启动动画
    if (animation->start(_simulationTime, startTime))
    {
        // 如果之前未激活，添加到活动动画列表
        if (!already_active) animations.push_back(animation);

        return true;
    }
    else
    {
        return false;
    }
}

// 停止指定动画
// 停止指定的动画并将其从活动动画列表中移除
// animation: 要停止的动画
// 返回值：true表示成功停止，false表示动画不在活动列表中
bool AnimationManager::stop(vsg::ref_ptr<vsg::Animation> animation)
{
    CPU_INSTRUMENTATION_L2_NC(instrumentation, "AnimationManager stop animation", COLOR_VIEWER);

    // 查找动画在列表中的位置
    auto itr = std::find(animations.begin(), animations.end(), animation);
    if (itr != animations.end())
    {
        // 停止动画并从列表中移除
        animation->stop(_simulationTime);
        animations.erase(itr);
        return true;
    }
    else
    {
        return false;
    }
}

// 停止所有动画
// 停止所有活动的动画并清空活动动画列表
// 返回值：true表示成功停止所有动画
bool AnimationManager::stop()
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "AnimationManager stop all animation", COLOR_VIEWER);

    // 停止所有动画
    for (auto& animation : animations)
    {
        animation->stop(_simulationTime);
    }
    // 清空活动动画列表
    animations.clear();
    return true;
}

// 更新动画
// 更新指定动画的状态
// animation: 要更新的动画
// 返回值：true表示动画仍在播放，false表示动画已结束
bool AnimationManager::update(vsg::Animation& animation)
{
    CPU_INSTRUMENTATION_L2_NC(instrumentation, "AnimationManager update animation", COLOR_VIEWER);
    return animation.update(_simulationTime);
}

// 运行动画更新
// 更新所有活动动画的状态，移除已结束的动画
// frameStamp: 帧戳对象，包含当前模拟时间
void AnimationManager::run(vsg::ref_ptr<vsg::FrameStamp> frameStamp)
{
    CPU_INSTRUMENTATION_L1_NC(instrumentation, "AnimationManager run animation updates", COLOR_VIEWER);

    // 更新模拟时间
    _simulationTime = frameStamp->simulationTime;

    // 更新所有活动动画，移除已结束的动画
    for (auto itr = animations.begin(); itr != animations.end();)
    {
        if (update(**itr))
            // 动画仍在播放，继续下一个
            ++itr;
        else
        {
            // 动画已结束，从列表中移除
            itr = animations.erase(itr);
        }
    }
}
