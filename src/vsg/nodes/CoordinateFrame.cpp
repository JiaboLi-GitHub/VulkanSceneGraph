/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/io/stream.h>
#include <vsg/maths/transform.h>
#include <vsg/nodes/CoordinateFrame.h>

using namespace vsg;

// 构造函数：创建坐标框架节点
// 坐标框架节点使用原点位置和旋转来定义局部坐标系，常用于地理空间应用
CoordinateFrame::CoordinateFrame()
{
}

// 拷贝构造函数：从另一个坐标框架节点创建新的坐标框架节点
// rhs: 要拷贝的坐标框架节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝名称、原点和旋转
CoordinateFrame::CoordinateFrame(const CoordinateFrame& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    name(rhs.name),
    origin(rhs.origin),
    rotation(rhs.rotation)
{
}

// 比较两个坐标框架节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类Transform、名称、原点和旋转
int CoordinateFrame::compare(const Object& rhs_object) const
{
    int result = Transform::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(name, rhs.name)) != 0) return result;
    if ((result = compare_value(origin, rhs.origin)) != 0) return result;
    return compare_value(rotation, rhs.rotation);
}

// 从输入流读取坐标框架节点对象
// input: 输入流对象
// 读取名称、原点、旋转、子图是否需要局部视锥体标志和子节点
void CoordinateFrame::read(Input& input)
{
    Node::read(input);
    input.read("name", name);
    input.read("origin", origin);
    input.read("rotation", rotation);
    input.read("subgraphRequiresLocalFrustum", subgraphRequiresLocalFrustum);
    input.readObjects("children", children);
}

// 将坐标框架节点对象写入输出流
// output: 输出流对象
// 写入名称、原点、旋转、子图是否需要局部视锥体标志和子节点
void CoordinateFrame::write(Output& output) const
{
    Node::write(output);
    output.write("name", name);
    output.write("origin", origin);
    output.write("rotation", rotation);
    output.write("subgraphRequiresLocalFrustum", subgraphRequiresLocalFrustum);
    output.writeObjects("children", children);
}

// 计算变换矩阵
// mv: 当前模型视图矩阵
// 返回: 应用坐标框架变换后的模型视图矩阵
// 先平移原点，然后应用旋转
dmat4 CoordinateFrame::transform(const dmat4& mv) const
{
    return mv * translate(dvec3(origin)) * rotate(rotation);
}
