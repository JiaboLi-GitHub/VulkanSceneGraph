/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/animation/AnimationGroup.h>
#include <vsg/animation/CameraSampler.h>
#include <vsg/animation/Joint.h>
#include <vsg/app/Camera.h>
#include <vsg/core/compare.h>
#include <vsg/io/Input.h>
#include <vsg/io/Output.h>
#include <vsg/nodes/MatrixTransform.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CameraKeyframes
//
CameraKeyframes::CameraKeyframes()
{
}

void CameraKeyframes::read(Input& input)
{
    Object::read(input);

    input.read("name", name);

    // read tracking key frames
    uint32_t num_tracking = input.readValue<uint32_t>("tracking");
    tracking.resize(num_tracking);
    for (auto& track : tracking)
    {
        input.matchPropertyName("track");
        input.read(1, &track.time);
        input.readObjects("path", track.value);
    }

    // read position key frames
    uint32_t num_positions = input.readValue<uint32_t>("positions");
    positions.resize(num_positions);
    for (auto& position : positions)
    {
        input.matchPropertyName("position");
        input.read(1, &position.time);
        input.read(1, &position.value);
    }

    // read rotation key frames
    uint32_t num_rotations = input.readValue<uint32_t>("rotations");
    rotations.resize(num_rotations);
    for (auto& rotation : rotations)
    {
        input.matchPropertyName("rotation");
        input.read(1, &rotation.time);
        input.read(1, &rotation.value);
    }

    // read field of view key frames
    uint32_t num_fieldOfViews = input.readValue<uint32_t>("fieldOfViews");
    fieldOfViews.resize(num_fieldOfViews);
    for (auto& fov : fieldOfViews)
    {
        input.matchPropertyName("fov");
        input.read(1, &fov.time);
        input.read(1, &fov.value);
    }

    // read near/far key frames
    uint32_t num_nearFars = input.readValue<uint32_t>("nearFars");
    nearFars.resize(num_nearFars);
    for (auto& nf : nearFars)
    {
        input.matchPropertyName("nearfar");
        input.read(1, &nf.time);
        input.read(1, &nf.value);
    }
}

void CameraKeyframes::write(Output& output) const
{
    Object::write(output);

    output.write("name", name);

    // write position key frames
    output.writeValue<uint32_t>("tracking", tracking.size());
    for (const auto& track : tracking)
    {
        output.writePropertyName("track");
        output.write(1, &track.time);
        output.writeEndOfLine();

        output.writeObjects("path", track.value);
    }

    // write position key frames
    output.writeValue<uint32_t>("positions", positions.size());
    for (const auto& position : positions)
    {
        output.writePropertyName("position");
        output.write(1, &position.time);
        output.write(1, &position.value);
        output.writeEndOfLine();
    }

    // write rotation key frames
    output.writeValue<uint32_t>("rotations", rotations.size());
    for (const auto& rotation : rotations)
    {
        output.writePropertyName("rotation");
        output.write(1, &rotation.time);
        output.write(1, &rotation.value);
        output.writeEndOfLine();
    }

    // write scale key frames
    for (const auto& scale : fieldOfViews)
    {
        output.writePropertyName("fov");
        output.write(1, &scale.time);
        output.write(1, &scale.value);
        output.writeEndOfLine();
    }

    // write field of view key frames
    output.writeValue<uint32_t>("fieldOfViews", fieldOfViews.size());
    for (const auto& fov : fieldOfViews)
    {
        output.writePropertyName("fov");
        output.write(1, &fov.time);
        output.write(1, &fov.value);
        output.writeEndOfLine();
    }

    // read near/far key frames
    output.writeValue<uint32_t>("nearFars", nearFars.size());
    for (const auto& nf : nearFars)
    {
        output.writePropertyName("nearfar");
        output.write(1, &nf.time);
        output.write(1, &nf.value);
        output.writeEndOfLine();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CameraSampler - 相机采样器，用于相机动画
//

// CameraSampler类的默认构造函数
// 创建相机采样器，用于在特定时间点采样相机动画关键帧
CameraSampler::CameraSampler() :
    origin(0.0, 0.0, 0.0),  // 初始原点
    position(0.0, 0.0, 0.0),  // 初始位置
    rotation(),  // 初始旋转（单位四元数）
    fieldOfView(60.0),  // 初始视野角度（度）
    nearFar(1.0, 1e10)  // 初始近远平面距离
{
}

// CameraSampler类的拷贝构造函数
// 使用CopyOp参数来支持深度拷贝操作
// rhs: 要拷贝的源对象
// copyop: 拷贝操作选项
CameraSampler::CameraSampler(const CameraSampler& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    keyframes(copyop(rhs.keyframes)),  // 相机关键帧
    object(copyop(rhs.object)),  // 要动画化的相机对象
    position(rhs.position),  // 当前位置
    rotation(rhs.rotation),  // 当前旋转
    fieldOfView(rhs.fieldOfView),  // 当前视野角度
    nearFar(rhs.nearFar)  // 当前近远平面距离
{
}

// 比较两个CameraSampler对象
// 首先比较基类，然后比较关键帧和对象
// 返回值：0表示相等，-1表示当前对象小于rhs，1表示当前对象大于rhs
int CameraSampler::compare(const Object& rhs_object) const
{
    int result = AnimationSampler::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    // 比较关键帧
    if ((result = compare_pointer(keyframes, rhs.keyframes)) != 0) return result;
    // 比较对象
    return compare_pointer(object, rhs.object);
}

// 更新相机采样器
// 根据时间采样关键帧，更新相机参数，并应用到相机对象
// time: 动画时间
void CameraSampler::update(double time)
{
    if (keyframes)
    {
        // 采样各种相机参数的关键帧
        sample(time, keyframes->origins, origin);  // 采样原点
        sample(time, keyframes->positions, position);  // 采样位置
        sample(time, keyframes->rotations, rotation);  // 采样旋转
        sample(time, keyframes->fieldOfViews, fieldOfView);  // 采样视野角度
        sample(time, keyframes->nearFars, nearFar);  // 采样近远平面距离

        // 从对象路径中提取变换值的辅助函数
        auto find_values = [](const RefObjectPath& path, dvec3& in_origin, dvec3& in_position, dquat& in_rotation) -> void {
            ComputeTransform ct;
            // 遍历路径中的所有对象，计算累积变换
            for (auto& obj : path) obj->accept(ct);

            in_origin = ct.origin;

            // 分解变换矩阵为位置、旋转和缩放
            dvec3 scale;
            vsg::decompose(ct.matrix, in_position, in_rotation, scale);
        };

        // 处理跟踪路径（tracking）关键帧
        auto& tracking = keyframes->tracking;
        if (tracking.size() == 1)
        {
            // 只有一个跟踪关键帧，直接使用
            find_values(tracking.front().value, origin, position, rotation);
        }
        else if (!tracking.empty())
        {
            // 多个跟踪关键帧，进行插值
            // 找到当前时间对应的关键帧位置
            auto pos_itr = std::lower_bound(tracking.begin(), tracking.end(), time, [](const time_path& elem, double t) -> bool { return elem.time < t; });
            if (pos_itr == tracking.begin())
            {
                // 时间在第一个关键帧之前，使用第一个关键帧
                find_values(tracking.front().value, origin, position, rotation);
            }
            else if (pos_itr == tracking.end())
            {
                // 时间在最后一个关键帧之后，使用最后一个关键帧
                find_values(tracking.back().value, origin, position, rotation);
            }
            else
            {
                // 时间在两个关键帧之间，进行插值
                auto before_pos_itr = pos_itr - 1;
                double delta_time = (pos_itr->time - before_pos_itr->time);
                double r = delta_time != 0.0 ? (time - before_pos_itr->time) / delta_time : 0.5;

                // 计算前后两个关键帧的变换值
                dvec3 origin_before, position_before;
                dquat rotation_before;
                find_values(before_pos_itr->value, origin_before, position_before, rotation_before);

                dvec3 origin_after, position_after;
                dquat rotation_after;
                find_values(pos_itr->value, origin_after, position_after, rotation_after);

                // 使用long double进行插值以最小化中间舍入误差
                origin = mix(ldvec3(origin_before), ldvec3(origin_after), static_cast<long double>(r));

                // 插值位置和旋转
                position = mix(position_before, position_after, r);
                rotation = mix(rotation_before, rotation_after, r);
            }
        }
    }

    // 将采样结果应用到相机对象
    if (object) object->accept(*this);
}

// 计算最大时间
// 返回相机关键帧的最大时间（所有关键帧类型中的最大值）
// 返回值：最大时间，如果没有关键帧则返回0
double CameraSampler::maxTime() const
{
    double maxTime = 0.0;
    if (keyframes)
    {
        // 查找所有关键帧类型中的最大时间
        if (!keyframes->tracking.empty()) maxTime = std::max(maxTime, keyframes->tracking.back().time);
        if (!keyframes->origins.empty()) maxTime = std::max(maxTime, keyframes->origins.back().time);
        if (!keyframes->positions.empty()) maxTime = std::max(maxTime, keyframes->positions.back().time);
        if (!keyframes->rotations.empty()) maxTime = std::max(maxTime, keyframes->rotations.back().time);
        if (!keyframes->fieldOfViews.empty()) maxTime = std::max(maxTime, keyframes->fieldOfViews.back().time);
        if (!keyframes->nearFars.empty()) maxTime = std::max(maxTime, keyframes->nearFars.back().time);
    }

    return maxTime;
}

// 应用相机采样器到mat4Value对象
// 将变换矩阵应用到float类型的4x4矩阵值
void CameraSampler::apply(mat4Value& matrix)
{
    matrix.set(mat4(transform()));
}

// 应用相机采样器到dmat4Value对象
// 将变换矩阵应用到double类型的4x4矩阵值
void CameraSampler::apply(dmat4Value& matrix)
{
    matrix.set(transform());
}

// 应用相机采样器到LookAt对象
// 将相机参数应用到LookAt视图矩阵
void CameraSampler::apply(LookAt& lookAt)
{
    if (keyframes)
    {
        bool has_tracking = !keyframes->tracking.empty();
        // 如果有原点关键帧或跟踪路径，设置原点
        if (!keyframes->origins.empty() || has_tracking) lookAt.origin = origin;
        // 如果有位置、旋转关键帧或跟踪路径，设置变换
        if (!keyframes->positions.empty() || !keyframes->rotations.empty() || has_tracking)
        {
            lookAt.set(transform());
        }
    }
}

// 应用相机采样器到LookDirection对象
// 将相机参数应用到LookDirection视图矩阵
void CameraSampler::apply(LookDirection& lookDirection)
{
    if (keyframes)
    {
        bool has_tracking = !keyframes->tracking.empty();
        // 设置原点、位置和旋转
        if (!keyframes->origins.empty() || has_tracking) lookDirection.origin = origin;
        if (!keyframes->positions.empty() || has_tracking) lookDirection.position = position;
        if (!keyframes->rotations.empty() || has_tracking) lookDirection.rotation = rotation;
    }
}

// 应用相机采样器到Perspective对象
// 将视野角度和近远平面距离应用到透视投影矩阵
void CameraSampler::apply(Perspective& perspective)
{
    // 设置视野角度
    if (keyframes && !keyframes->fieldOfViews.empty())
    {
        perspective.fieldOfViewY = fieldOfView;
    }
    // 设置近远平面距离
    if (keyframes && !keyframes->nearFars.empty())
    {
        perspective.nearDistance = nearFar[0];
        perspective.farDistance = nearFar[1];
    }
}

// 应用相机采样器到Camera对象
// 将相机参数应用到相机的投影矩阵和视图矩阵
void CameraSampler::apply(Camera& camera)
{
    // 应用到投影矩阵和视图矩阵
    if (camera.projectionMatrix) camera.projectionMatrix->accept(*this);
    if (camera.viewMatrix) camera.viewMatrix->accept(*this);
}

// 从输入流读取CameraSampler对象
// 读取相机关键帧和对象
void CameraSampler::read(Input& input)
{
    AnimationSampler::read(input);
    input.read("keyframes", keyframes);
    input.read("object", object);
}

// 将CameraSampler对象写入输出流
// 写入相机关键帧和对象
void CameraSampler::write(Output& output) const
{
    AnimationSampler::write(output);
    output.write("keyframes", keyframes);
    output.write("object", object);
}
