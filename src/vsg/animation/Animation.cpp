/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/Animation.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/nodes/MatrixTransform.h>

using namespace vsg;

// AnimationSampler类的默认构造函数
// 创建动画采样器，用于在特定时间点采样动画关键帧
AnimationSampler::AnimationSampler()
{
}

// AnimationSampler类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
AnimationSampler::AnimationSampler(const AnimationSampler& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    name(rhs.name)  // 采样器名称
{
}

// 比较两个AnimationSampler对象
// 首先比较基类，然后比较名称
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int AnimationSampler::compare(const Object& rhs_object) const
{
    int result = Visitor::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较名称
    return compare_value(name, rhs.name);
}

// 从输入流读取AnimationSampler对象
// 读取采样器名称
void AnimationSampler::read(Input& input)
{
    Object::read(input);
    input.read("name", name);
}

// 将AnimationSampler对象写入输出流
// 写入采样器名称
void AnimationSampler::write(Output& output) const
{
    Object::write(output);
    output.write("name", name);
}

// Animation类的默认构造函数
// 创建动画对象，包含多个采样器，用于控制场景中对象的动画
Animation::Animation()
{
}

// Animation类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
Animation::Animation(const Animation& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    name(rhs.name),  // 动画名称
    mode(rhs.mode),  // 动画模式（播放一次、循环、往返等）
    time(rhs.time),  // 当前动画时间
    speed(rhs.speed),  // 播放速度
    samplers(copyop(rhs.samplers)),  // 动画采样器列表
    _active(false),  // 动画是否激活（拷贝后重置为未激活）
    _previousSimulationTime(rhs._previousSimulationTime),  // 上一次模拟时间
    _maxTime(rhs._maxTime)  // 动画最大时间
{
}

// 比较两个Animation对象
// 首先比较基类，然后比较名称、模式、速度和采样器列表
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int Animation::compare(const Object& rhs_object) const
{
    int result = Object::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较名称
    if ((result = compare_value(name, rhs.name)) != 0) return result;
    // 比较模式
    if ((result = compare_value(mode, rhs.mode)) != 0) return result;
    // 比较速度
    if ((result = compare_value(speed, rhs.speed)) != 0) return result;
    // 比较采样器列表
    return compare_pointer_container(samplers, rhs.samplers);
}

// 从输入流读取Animation对象
// 读取动画的名称、模式、速度和采样器列表
void Animation::read(Input& input)
{
    Object::read(input);

    // 读取动画属性
    input.read("name", name);  // 动画名称
    input.readValue<uint32_t>("mode", mode);  // 动画模式
    input.read("speed", speed);  // 播放速度
    input.readObjects("samplers", samplers);  // 采样器列表
}

// 将Animation对象写入输出流
// 写入动画的名称、模式、速度和采样器列表
void Animation::write(Output& output) const
{
    Object::write(output);

    // 写入动画属性
    output.write("name", name);
    output.writeValue<uint32_t>("mode", mode);
    output.write("speed", speed);
    output.writeObjects("samplers", samplers);
}

// 计算动画的最大时间
// 遍历所有采样器，找到最大的时间值
// 返回值：动画的最大时间
double Animation::maxTime() const
{
    double mt = 0.0;
    for (auto sampler : samplers)
    {
        mt = std::max(mt, sampler->maxTime());
    }
    return mt;
}

// 启动动画
// 初始化动画状态，设置起始时间和模拟时间
// simulationTime: 当前模拟时间
// startTime: 动画起始时间
// 返回值：true表示成功启动，false表示启动失败（如没有采样器）
bool Animation::start(double simulationTime, double startTime)
{
    // 设置动画时间和上一次模拟时间
    time = startTime;
    _previousSimulationTime = simulationTime;

    // 如果没有采样器，无法启动动画
    if (samplers.empty())
    {
        _active = false;
        return false;
    }

    // 缓存最大时间，避免在更新时重复计算
    _maxTime = maxTime();

    // 激活动画
    _active = true;
    return _active;
}

// 更新动画
// 根据模拟时间更新动画状态，处理不同的播放模式
// simulationTime: 当前模拟时间
// 返回值：true表示动画仍在播放，false表示动画已结束
bool Animation::update(double simulationTime)
{
    // 如果动画未激活，直接返回
    if (!_active) return false;

    bool finished = false;

    // 计算时间在周期内的位置（用于循环和往返模式）
    auto time_within_period = [](double x, double y) -> double {
        return x < 0.0 ? y + std::fmod(x, y) : std::fmod(x, y);
    };

    // 根据时间差和速度更新动画时间
    auto samplerTime = time = time + (simulationTime - _previousSimulationTime) * speed;

    _previousSimulationTime = simulationTime;

    // 根据动画模式处理时间
    if (mode == REPEAT)
    {
        // 循环模式：时间在0到_maxTime之间循环
        samplerTime = time = time_within_period(time, _maxTime);
    }
    else if (mode == FORWARD_AND_BACK)
    {
        // 往返模式：先正向播放，然后反向播放
        samplerTime = time = time_within_period(time, 2.0 * _maxTime);
        if (time > _maxTime) samplerTime = 2.0 * _maxTime - time;  // 反向播放时反转时间
    }
    else
    {
        // 播放一次模式：如果超过最大时间，动画结束
        if (time > _maxTime)
        {
            finished = true;
            samplerTime = time = _maxTime;
        }
    }

    // 更新所有采样器
    for (auto sampler : samplers)
    {
        sampler->update(samplerTime);
    }

    // 如果动画已结束，停止动画
    if (finished)
    {
        stop(simulationTime);
        return false;
    }

    return true;
}

// 停止动画
// 将动画设置为非激活状态
// simulationTime: 当前模拟时间（未使用）
// 返回值：false表示动画已停止
bool Animation::stop(double /*simulationTime*/)
{
    _active = false;
    return false;
}
