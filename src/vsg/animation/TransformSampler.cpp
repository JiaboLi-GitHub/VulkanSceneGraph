/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/AnimationGroup.h>
#include <vsg/animation/Joint.h>
#include <vsg/animation/TransformSampler.h>
#include <vsg/app/Camera.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/nodes/MatrixTransform.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TransformKeyframes - 变换关键帧，用于存储位置、旋转和缩放的关键帧数据
//

// TransformKeyframes类的默认构造函数
// 创建变换关键帧对象，用于存储变换动画的关键帧数据
TransformKeyframes::TransformKeyframes()
{
}

// 从输入流读取TransformKeyframes对象
// 读取变换关键帧的名称和位置、旋转、缩放关键帧数据
void TransformKeyframes::read(Input& input)
{
    Object::read(input);

    // 读取名称
    input.read("name", name);

    // 读取位置关键帧
    uint32_t num_positions = input.readValue<uint32_t>("positions");
    positions.resize(num_positions);
    for (auto& position : positions)
    {
        input.matchPropertyName("position");
        input.read(1, &position.time);  // 关键帧时间
        input.read(1, &position.value);  // 位置值
    }

    // 读取旋转关键帧
    uint32_t num_rotations = input.readValue<uint32_t>("rotations");
    rotations.resize(num_rotations);
    for (auto& rotation : rotations)
    {
        input.matchPropertyName("rotation");
        input.read(1, &rotation.time);  // 关键帧时间
        input.read(1, &rotation.value);  // 旋转值（四元数）
    }

    // 读取缩放关键帧
    uint32_t num_scales = input.readValue<uint32_t>("scales");
    scales.resize(num_scales);
    for (auto& scale : scales)
    {
        input.matchPropertyName("scale");
        input.read(1, &scale.time);  // 关键帧时间
        input.read(1, &scale.value);  // 缩放值
    }
}

// 将TransformKeyframes对象写入输出流
// 写入变换关键帧的名称和位置、旋转、缩放关键帧数据
void TransformKeyframes::write(Output& output) const
{
    Object::write(output);

    // 写入名称
    output.write("name", name);

    // 写入位置关键帧
    output.writeValue<uint32_t>("positions", positions.size());
    for (const auto& position : positions)
    {
        output.writePropertyName("position");
        output.write(1, &position.time);
        output.write(1, &position.value);
        output.writeEndOfLine();
    }

    // 写入旋转关键帧
    output.writeValue<uint32_t>("rotations", rotations.size());
    for (const auto& rotation : rotations)
    {
        output.writePropertyName("rotation");
        output.write(1, &rotation.time);
        output.write(1, &rotation.value);
        output.writeEndOfLine();
    }

    // 写入缩放关键帧
    output.writeValue<uint32_t>("scales", scales.size());
    for (const auto& scale : scales)
    {
        output.writePropertyName("scale");
        output.write(1, &scale.time);
        output.write(1, &scale.value);
        output.writeEndOfLine();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TransformSampler - 变换采样器，用于变换动画
//

// TransformSampler类的默认构造函数
// 创建变换采样器，用于在特定时间点采样变换动画关键帧
TransformSampler::TransformSampler() :
    position(0.0, 0.0, 0.0),  // 初始位置
    rotation(),  // 初始旋转（单位四元数）
    scale(1.0, 1.0, 1.0)  // 初始缩放
{
}

// TransformSampler类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
TransformSampler::TransformSampler(const TransformSampler& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    keyframes(copyop(rhs.keyframes)),  // 变换关键帧
    object(copyop(rhs.object)),  // 要变换的对象
    position(rhs.position),  // 当前位置
    rotation(rhs.rotation),  // 当前旋转
    scale(rhs.scale)  // 当前缩放
{
}

// 比较两个TransformSampler对象
// 首先比较基类，然后比较关键帧和对象
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int TransformSampler::compare(const Object& rhs_object) const
{
    int result = AnimationSampler::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较关键帧
    if ((result = compare_pointer(keyframes, rhs.keyframes)) != 0) return result;
    // 比较对象
    return compare_pointer(object, rhs.object);
}

// 更新变换采样器
// 根据时间采样关键帧，更新位置、旋转和缩放，并应用到对象
// time: 动画时间
void TransformSampler::update(double time)
{
    if (keyframes)
    {
        // 采样位置、旋转和缩放关键帧
        sample(time, keyframes->positions, position);
        sample(time, keyframes->rotations, rotation);
        sample(time, keyframes->scales, scale);
    }

    // 将变换应用到对象
    if (object) object->accept(*this);
}

// 计算最大时间
// 返回变换关键帧的最大时间（位置、旋转、缩放中的最大值）
// 返回值：最大时间，如果没有关键帧则返回0
double TransformSampler::maxTime() const
{
    double maxTime = 0.0;
    if (keyframes)
    {
        // 查找位置、旋转、缩放关键帧中的最大时间
        if (!keyframes->positions.empty()) maxTime = std::max(maxTime, keyframes->positions.back().time);
        if (!keyframes->rotations.empty()) maxTime = std::max(maxTime, keyframes->rotations.back().time);
        if (!keyframes->scales.empty()) maxTime = std::max(maxTime, keyframes->scales.back().time);
    }

    return maxTime;
}

// 应用变换采样器到mat4Value对象
// 将变换矩阵应用到float类型的4x4矩阵值
void TransformSampler::apply(mat4Value& matrix)
{
    matrix.set(mat4(transform()));
}

// 应用变换采样器到dmat4Value对象
// 将变换矩阵应用到double类型的4x4矩阵值
void TransformSampler::apply(dmat4Value& matrix)
{
    matrix.set(transform());
}

// 应用变换采样器到MatrixTransform对象
// 将变换矩阵应用到矩阵变换节点
void TransformSampler::apply(MatrixTransform& mt)
{
    mt.matrix.set(transform());
}

// 应用变换采样器到Joint对象
// 将变换矩阵应用到关节节点
void TransformSampler::apply(Joint& joint)
{
    joint.matrix.set(transform());
}

// 应用变换采样器到LookAt对象
// 将变换应用到LookAt视图矩阵
void TransformSampler::apply(LookAt& lookAt)
{
    lookAt.set(transform());
}

// 应用变换采样器到Camera对象
// 将变换应用到相机的视图矩阵
void TransformSampler::apply(Camera& camera)
{
    if (camera.viewMatrix)
    {
        camera.viewMatrix->accept(*this);
    }
}

// 从输入流读取TransformSampler对象
// 读取变换关键帧和对象
void TransformSampler::read(Input& input)
{
    AnimationSampler::read(input);
    input.read("keyframes", keyframes);
    input.read("object", object);
}

// 将TransformSampler对象写入输出流
// 写入变换关键帧和对象
void TransformSampler::write(Output& output) const
{
    AnimationSampler::write(output);
    output.write("keyframes", keyframes);
    output.write("object", object);
}
