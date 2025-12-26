/* <editor-fold desc="MIT License">

Copyright(c) 2024 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/stream.h>
#include <vsg/nodes/RegionOfInterest.h>

using namespace vsg;

// 构造函数：创建感兴趣区域节点
// 感兴趣区域节点定义了一个3D空间区域（由点集定义），用于标记和识别场景中的特定区域
RegionOfInterest::RegionOfInterest()
{
}

// 拷贝构造函数：从另一个感兴趣区域节点创建新的感兴趣区域节点
// rhs: 要拷贝的感兴趣区域节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝掩码、名称和点集
RegionOfInterest::RegionOfInterest(const RegionOfInterest& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    mask(rhs.mask),
    name(rhs.name),
    points(rhs.points)
{
}

// 析构函数：销毁感兴趣区域节点
RegionOfInterest::~RegionOfInterest()
{
}

// 比较两个感兴趣区域节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 依次比较基类、掩码、名称和点集
int RegionOfInterest::compare(const Object& rhs_object) const
{
    int result = Node::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    if ((result = compare_value(mask, rhs.mask)) != 0) return result;
    if ((result = compare_value(name, rhs.name)) != 0) return result;
    return compare_value_container(points, rhs.points);
}

// 从输入流读取感兴趣区域节点对象
// input: 输入流对象
// 读取掩码、名称和点集
void RegionOfInterest::read(Input& input)
{
    Node::read(input);

    input.read("mask", mask);
    input.read("name", name);
    input.read("points", points);
}

// 将感兴趣区域节点对象写入输出流
// output: 输出流对象
// 写入掩码、名称和点集
void RegionOfInterest::write(Output& output) const
{
    Node::write(output);

    output.write("mask", mask);
    output.write("name", name);
    output.write("points", points);
}
