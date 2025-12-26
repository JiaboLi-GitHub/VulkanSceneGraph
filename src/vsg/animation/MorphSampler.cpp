/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/MorphSampler.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Logger.h>
#include <vsg/io/Output.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MorphKeyframes - 形变动画关键帧
//

// MorphKeyframes类的默认构造函数
// 创建形变关键帧对象，用于存储形变动画的关键帧数据
MorphKeyframes::MorphKeyframes()
{
}

// 从输入流读取MorphKeyframes对象
// 读取形变关键帧的名称和关键帧数据
void MorphKeyframes::read(Input& input)
{
    Object::read(input);

    // 读取名称
    input.read("name", name);

    // 读取关键帧数据
    uint32_t num_keyframes = input.readValue<uint32_t>("keyframes");
    keyframes.resize(num_keyframes);
    for (auto& keyframe : keyframes)
    {
        input.read("time", keyframe.time);  // 关键帧时间
        input.readValues("values", keyframe.values);  // 形变值数组
        input.readValues("weights", keyframe.weights);  // 形变权重数组
    }
}

// 将MorphKeyframes对象写入输出流
// 写入形变关键帧的名称和关键帧数据
void MorphKeyframes::write(Output& output) const
{
    Object::write(output);

    // 写入名称
    output.write("name", name);

    // 写入关键帧数据
    output.writeValue<uint32_t>("keyFrames", keyframes.size());
    for (auto& keyframe : keyframes)
    {
        output.write("time", keyframe.time);
        output.writeValues("values", keyframe.values);
        output.writeValues("weights", keyframe.weights);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MorphSampler - 形变采样器，用于形变动画
//

// MorphSampler类的默认构造函数
// 创建形变采样器，用于在特定时间点采样形变动画关键帧
MorphSampler::MorphSampler()
{
}

// MorphSampler类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
MorphSampler::MorphSampler(const MorphSampler& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    keyframes(copyop(rhs.keyframes)),  // 形变关键帧
    object(copyop(rhs.object))  // 要形变的对象
{
}

// 比较两个MorphSampler对象
// 首先比较基类，然后比较关键帧和对象
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int MorphSampler::compare(const Object& rhs_object) const
{
    int result = AnimationSampler::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较关键帧
    if ((result = compare_pointer(keyframes, rhs.keyframes)) != 0) return result;
    // 比较对象
    return compare_pointer(object, rhs.object);
}

// 更新形变采样器
// 注意：此功能尚未实现
// time: 动画时间
void MorphSampler::update(double /*time*/)
{
    // TODO write implementation of passing morph values to associated scene graph data structures
    vsg::warn("MorphSampler::update(double time) not implemented yet");
}

// 计算最大时间
// 返回形变关键帧的最大时间
// 返回值：最大时间，如果没有关键帧则返回0
double MorphSampler::maxTime() const
{
    double maxTime = 0.0;
    if (keyframes && !keyframes->keyframes.empty())
    {
        maxTime = std::max(maxTime, keyframes->keyframes.back().time);
    }
    return maxTime;
}

// 从输入流读取MorphSampler对象
// 读取形变关键帧和对象
void MorphSampler::read(Input& input)
{
    AnimationSampler::read(input);
    input.read("keyframes", keyframes);
    input.read("object", object);
}

// 将MorphSampler对象写入输出流
// 写入形变关键帧和对象
void MorphSampler::write(Output& output) const
{
    AnimationSampler::write(output);
    output.write("keyframes", keyframes);
    output.write("object", object);
}
