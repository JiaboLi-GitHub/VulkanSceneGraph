/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/compare.h>
#include <vsg/io/stream.h>
#include <vsg/nodes/Transform.h>

using namespace vsg;

// 构造函数：创建变换节点（基类）
// 变换节点是用于应用空间变换的组节点的基类
Transform::Transform()
{
}

// 拷贝构造函数：从另一个变换节点创建新的变换节点
// rhs: 要拷贝的变换节点对象
// copyop: 拷贝操作参数，用于控制深度拷贝行为
// 拷贝子图是否需要局部视锥体标志
Transform::Transform(const Transform& rhs, const CopyOp& copyop) :
    Inherit(rhs, copyop),
    subgraphRequiresLocalFrustum(rhs.subgraphRequiresLocalFrustum)
{
}

// 比较两个变换节点对象
// rhs_object: 要比较的对象
// 返回: 比较结果，0表示相等，负数表示小于，正数表示大于
// 首先比较基类Group，然后比较子图是否需要局部视锥体标志
int Transform::compare(const Object& rhs_object) const
{
    int result = Group::compare(rhs_object);
    if (result != 0) return result;

    const auto& rhs = static_cast<decltype(*this)>(rhs_object);
    return compare_value(subgraphRequiresLocalFrustum, rhs.subgraphRequiresLocalFrustum);
}

// 从输入流读取变换节点对象
// input: 输入流对象
// 读取子节点（通过基类Group）
void Transform::read(Input& input)
{
    Group::read(input);
}

// 将变换节点对象写入输出流
// output: 输出流对象
// 写入子节点（通过基类Group）
void Transform::write(Output& output) const
{
    Group::write(output);
}
