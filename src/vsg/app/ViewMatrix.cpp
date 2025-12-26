/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/ViewMatrix.h>
#include <vsg/maths/transform.h>

using namespace vsg;

// 从输入流读取视图矩阵数据
// 读取原点（如果版本支持）
// input: 输入流对象
void ViewMatrix::read(Input& input)
{
    Object::read(input);

    // 从版本1.1.9开始支持原点
    if (input.version_greater_equal(1, 1, 9))
        input.read("origin", origin);
    else
        origin = {};  // 旧版本不读取原点
}

// 将视图矩阵数据写入输出流
// 写入原点（如果版本支持）
// output: 输出流对象
void ViewMatrix::write(Output& output) const
{
    Object::write(output);

    // 从版本1.1.9开始支持原点
    if (output.version_greater_equal(1, 1, 9))
        output.write("origin", origin);
}

// 从输入流读取LookAt视图矩阵数据
// 读取眼睛位置、中心点和上向量
// input: 输入流对象
void LookAt::read(Input& input)
{
    ViewMatrix::read(input);

    input.read("eye", eye);  // 眼睛位置（相机位置）
    input.read("center", center);  // 中心点（观察目标）
    input.read("up", up);  // 上向量
}

// 将LookAt视图矩阵数据写入输出流
// 写入眼睛位置、中心点和上向量
// output: 输出流对象
void LookAt::write(Output& output) const
{
    ViewMatrix::write(output);

    output.write("eye", eye);
    output.write("center", center);
    output.write("up", up);
}

// 应用变换矩阵到LookAt参数
// 使用给定的变换矩阵变换眼睛位置、中心点和上向量
// matrix: 要应用的变换矩阵
void LookAt::transform(const dmat4& matrix)
{
    // 变换上向量（保持归一化）
    up = normalize(matrix * (eye + up) - matrix * eye);
    // 变换中心点
    center = matrix * center;
    // 变换眼睛位置
    eye = matrix * eye;
}

// 从变换矩阵设置LookAt参数
// 从给定的变换矩阵提取眼睛位置、中心点和上向量
// matrix: 变换矩阵
void LookAt::set(const dmat4& matrix)
{
    // 从标准位置和上向量计算变换后的上向量
    up = normalize(matrix * (dvec3(0.0, 0.0, 0.0) + dvec3(0.0, 1.0, 0.0)) - matrix * dvec3(0.0, 0.0, 0.0));
    // 从标准前方向计算变换后的中心点
    center = matrix * dvec3(0.0, 0.0, -1.0);
    // 从原点计算变换后的眼睛位置
    eye = matrix * dvec3(0.0, 0.0, 0.0);
}

// 计算带偏移的LookAt变换矩阵
// 根据给定的偏移量计算视图变换矩阵
// offset: 偏移量
// 返回值：视图变换矩阵
dmat4 LookAt::transform(const dvec3& offset) const
{
    // 计算从原点到偏移量的差值
    dvec3 delta = dvec3(origin - offset);
    // 应用偏移量并计算LookAt矩阵
    return vsg::lookAt(eye + delta, center + delta, up);
}

// 从变换矩阵设置LookDirection参数
// 从给定的变换矩阵分解出位置和旋转
// matrix: 变换矩阵
void LookDirection::set(const dmat4& matrix)
{
    dvec3 scale;
    // 分解矩阵为位置、旋转和缩放
    vsg::decompose(matrix, position, rotation, scale);
}

// 计算带偏移的LookDirection变换矩阵
// 根据给定的偏移量计算视图变换矩阵
// offset: 偏移量
// 返回值：视图变换矩阵
dmat4 LookDirection::transform(const dvec3& offset) const
{
    // 先旋转（反向），然后平移（考虑偏移和原点）
    return vsg::rotate(-rotation) * vsg::translate(dvec3(offset - origin) - position);
}

// 计算相对视图矩阵的变换
// 将基础视图矩阵的变换与额外的矩阵组合
// offset: 偏移量
// 返回值：组合后的视图变换矩阵
dmat4 RelativeViewMatrix::transform(const dvec3& offset) const
{
    return matrix * viewMatrix->transform(offset);
}

// 计算跟踪视图矩阵的变换
// 计算跟踪场景图中对象的视图变换矩阵
// offset: 偏移量
// 返回值：跟踪视图变换矩阵
dmat4 TrackingViewMatrix::transform(const dvec3& offset) const
{
    // 组合矩阵：先应用偏移，然后计算对象路径的逆变换，最后应用基础矩阵
    return matrix * vsg::translate(dvec3(offset - origin)) * vsg::inverse(computeTransform(objectPath));
}

// 计算跟踪视图矩阵的逆变换
// 计算跟踪视图矩阵的逆变换
// offset: 偏移量
// 返回值：逆视图变换矩阵
dmat4 TrackingViewMatrix::inverse(const dvec3& offset) const
{
    // 逆变换：先应用基础矩阵的逆，然后应用偏移，最后计算对象路径的变换
    return vsg::computeTransform(objectPath) * vsg::translate(dvec3(offset - origin)) * vsg::inverse(matrix);
}
