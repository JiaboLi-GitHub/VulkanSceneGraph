/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/ProjectionMatrix.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Perspective - 透视投影矩阵
//

// 从输入流读取透视投影参数
// 读取垂直视场角、宽高比、近平面距离和远平面距离
// input: 输入流对象
void Perspective::read(Input& input)
{
    ProjectionMatrix::read(input);

    input.read("fieldOfViewY", fieldOfViewY);  // 垂直视场角（弧度）
    input.read("aspectRatio", aspectRatio);  // 宽高比
    input.read("nearDistance", nearDistance);  // 近平面距离
    input.read("farDistance", farDistance);  // 远平面距离
}

// 将透视投影参数写入输出流
// 写入垂直视场角、宽高比、近平面距离和远平面距离
// output: 输出流对象
void Perspective::write(Output& output) const
{
    ProjectionMatrix::write(output);

    output.write("fieldOfViewY", fieldOfViewY);
    output.write("aspectRatio", aspectRatio);
    output.write("nearDistance", nearDistance);
    output.write("farDistance", farDistance);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Orthographic - 正交投影矩阵
//

// 从输入流读取正交投影参数
// 读取左、右、底、顶、近平面距离和远平面距离
// input: 输入流对象
void Orthographic::read(Input& input)
{
    ProjectionMatrix::read(input);

    input.read("left", left);  // 左边界
    input.read("right", right);  // 右边界
    input.read("bottom", bottom);  // 底边界
    input.read("top", top);  // 顶边界
    input.read("nearDistance", nearDistance);  // 近平面距离
    input.read("farDistance", farDistance);  // 远平面距离
}

// 将正交投影参数写入输出流
// 写入左、右、底、顶、近平面距离和远平面距离
// output: 输出流对象
void Orthographic::write(Output& output) const
{
    ProjectionMatrix::write(output);

    output.write("left", left);
    output.write("right", right);
    output.write("bottom", bottom);
    output.write("top", top);
    output.write("nearDistance", nearDistance);
    output.write("farDistance", farDistance);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// EllipsoidPerspective - 椭球透视投影矩阵（用于地球渲染）
//

// 从输入流读取椭球透视投影参数
// 读取LookAt视图、椭球模型、垂直视场角、宽高比、近远平面比例和地平线山高
// input: 输入流对象
void EllipsoidPerspective::read(Input& input)
{
    ProjectionMatrix::read(input);

    input.read("lookAt", lookAt);  // LookAt视图矩阵
    input.read("ellipsoidModel", ellipsoidModel);  // 椭球模型
    input.read("fieldOfViewY", fieldOfViewY);  // 垂直视场角（弧度）
    input.read("aspectRatio", aspectRatio);  // 宽高比
    input.read("nearFarRatio", nearFarRatio);  // 近远平面比例
    input.read("horizonMountainHeight", horizonMountainHeight);  // 地平线山高（米）
}

// 将椭球透视投影参数写入输出流
// 写入LookAt视图、椭球模型、垂直视场角、宽高比、近远平面比例和地平线山高
// output: 输出流对象
void EllipsoidPerspective::write(Output& output) const
{
    ProjectionMatrix::write(output);

    output.write("lookAt", lookAt);
    output.write("ellipsoidModel", ellipsoidModel);
    output.write("fieldOfViewY", fieldOfViewY);
    output.write("aspectRatio", aspectRatio);
    output.write("nearFarRatio", nearFarRatio);
    output.write("horizonMountainHeight", horizonMountainHeight);
}
